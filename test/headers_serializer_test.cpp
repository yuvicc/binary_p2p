#define BOOST_TEST_MODULE HeadersSerializer
#include <boost/test/unit_test.hpp>

#include "block_header.h"
#include "headers_parser.h"
#include "headers_serializer.h"
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

BlockHeader sample_header(std::uint8_t seed)
{
    return BlockHeader{
        .version = 0x2000'0000,
        .previous_block_hash = hash_of(seed),
        .merkle_root = hash_of(static_cast<std::uint8_t>(seed + 1)),
        .timestamp = 0x5f5e'0100U,
        .bits = 0x1707'1e2bU,
        .nonce = 0xDEAD'BEEFU,
    };
}

std::vector<std::byte> encode(const std::vector<BlockHeader>& headers)
{
    ByteWriter writer;
    serialize_headers_payload(writer, headers);
    return writer.take();
}

} // namespace

BOOST_AUTO_TEST_CASE(block_header_is_eighty_bytes)
{
    ByteWriter writer;
    serialize_block_header(writer, sample_header(0x11));
    BOOST_TEST(writer.size() == 80);
}

BOOST_AUTO_TEST_CASE(empty_headers)
{
    const auto bytes = encode({});
    BOOST_REQUIRE(bytes.size() == 1);
    BOOST_TEST(bytes[0] == std::byte{0x00}); // CompactSize count of zero
}

BOOST_AUTO_TEST_CASE(single_header_layout)
{
    const auto bytes = encode({sample_header(0x22)});

    // 1 (count) + 80 (header) + 1 (zero tx count)
    BOOST_REQUIRE(bytes.size() == 82);
    BOOST_TEST(bytes[0] == std::byte{0x01});   // one header
    BOOST_TEST(bytes[81] == std::byte{0x00});  // trailing zero tx count
}

// The core guarantee: serialize then parse recovers the original headers.
BOOST_AUTO_TEST_CASE(round_trips_through_parser)
{
    const std::vector<BlockHeader> headers{
        sample_header(0x01),
        sample_header(0x40),
        sample_header(0xFE),
    };

    const auto bytes = encode(headers);
    const auto parsed = parse_headers_payload(bytes);

    BOOST_REQUIRE(parsed.has_value());
    BOOST_TEST((*parsed == headers));
}

BOOST_AUTO_TEST_CASE(round_trips_empty_through_parser)
{
    const auto bytes = encode({});
    const auto parsed = parse_headers_payload(bytes);

    BOOST_REQUIRE(parsed.has_value());
    BOOST_TEST(parsed->empty());
}
