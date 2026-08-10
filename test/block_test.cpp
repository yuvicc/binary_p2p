#define BOOST_TEST_MODULE Block
#include <boost/test/unit_test.hpp>

#include "block_parser.h"
#include "block_serializer.h"
#include "headers_serializer.h"
#include "message.h"
#include "util/writer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <vector>

namespace boost::test_tools::tt_detail {
template<>
struct print_log_value<ParseError> {
    void operator()(std::ostream& os, ParseError e) const
    {
        os << static_cast<int>(e);
    }
};

template<>
struct print_log_value<std::byte> {
    void operator()(std::ostream& os, std::byte b) const
    {
        os << "0x" << std::hex << std::to_integer<int>(b);
    }
};
}

namespace {

std::array<std::byte, 32> hash_of(std::uint8_t fill)
{
    std::array<std::byte, 32> hash{};
    hash.fill(std::byte{fill});
    return hash;
}

BlockHeader sample_header()
{
    return BlockHeader{
        .version = 0x2000'0000,
        .previous_block_hash = hash_of(0x01),
        .merkle_root = hash_of(0x02),
        .timestamp = 1'700'000'000,
        .bits = 0x1707'1e2b,
        .nonce = 0xDEAD'BEEF,
    };
}

// Opaque transaction bytes; the block parser keeps these verbatim.
std::vector<std::byte> fake_transactions(std::size_t n)
{
    std::vector<std::byte> bytes(n);
    for (std::size_t i = 0; i < n; ++i) {
        bytes[i] = static_cast<std::byte>(i & 0xff);
    }
    return bytes;
}

std::vector<std::byte> encode(const BlockMessage& message)
{
    ByteWriter writer;
    serialize_block_payload(writer, message);
    return writer.take();
}

} // namespace

BOOST_AUTO_TEST_CASE(round_trips_through_parser)
{
    const BlockMessage message{
        .header = sample_header(),
        .transaction_count = 1,
        .transactions = fake_transactions(64),
    };

    const auto bytes = encode(message);
    // 80 (header) + 1 (count) + 64 (transactions)
    BOOST_REQUIRE(bytes.size() == 80 + 1 + 64);

    const auto parsed = parse_block_payload(bytes);
    BOOST_REQUIRE(parsed.has_value());
    BOOST_TEST((parsed->header == message.header));
    BOOST_TEST(parsed->transaction_count == 1U);
    BOOST_TEST((parsed->transactions == message.transactions));
    BOOST_TEST((*parsed == message));
}

// The serialized block body is exactly what a validation engine consumes: a
// header, the tx count, and the raw transactions.
BOOST_AUTO_TEST_CASE(header_precedes_transactions)
{
    const BlockMessage message{
        .header = sample_header(),
        .transaction_count = 2,
        .transactions = fake_transactions(10),
    };

    const auto bytes = encode(message);
    BOOST_TEST(bytes[80] == std::byte{0x02}); // tx count right after 80-byte header
}

BOOST_AUTO_TEST_CASE(header_only_truncation_is_malformed)
{
    // A valid 80-byte header but no transaction count following it.
    ByteWriter writer;
    serialize_block_header(writer, sample_header());
    const auto bytes = writer.take();

    const auto parsed = parse_block_payload(bytes);
    BOOST_REQUIRE(!parsed.has_value());
    BOOST_TEST(parsed.error() == ParseError::malformed_payload);
}

BOOST_AUTO_TEST_CASE(count_zero_with_trailing_bytes_is_rejected)
{
    BlockMessage message{
        .header = sample_header(),
        .transaction_count = 0,
        .transactions = fake_transactions(8), // inconsistent with count 0
    };

    const auto parsed = parse_block_payload(encode(message));
    BOOST_REQUIRE(!parsed.has_value());
    BOOST_TEST(parsed.error() == ParseError::trailing_bytes);
}
