#define BOOST_TEST_MODULE HeadersParser
#include <boost/test/unit_test.hpp>

#include "headers_parser.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <span>
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
} // namespace boost::test_tools::tt_detail

namespace {

void put_u8(std::vector<std::byte>& out, std::uint8_t value)
{
    out.push_back(std::byte{value});
}

template<class T>
void put_le(std::vector<std::byte>& out, T value)
{
    using U = std::make_unsigned_t<T>;
    const auto bits = static_cast<U>(value);
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        put_u8(out, static_cast<std::uint8_t>(bits >> (8 * i)));
    }
}

void put_compact_size(std::vector<std::byte>& out, std::uint64_t value)
{
    if (value < 0xfd) {
        put_u8(out, static_cast<std::uint8_t>(value));
    } else if (value <= 0xffff) {
        put_u8(out, 0xfd);
        put_u8(out, static_cast<std::uint8_t>(value));
        put_u8(out, static_cast<std::uint8_t>(value >> 8));
    } else if (value <= 0xffff'ffff) {
        put_u8(out, 0xfe);
        put_le<std::uint32_t>(out, static_cast<std::uint32_t>(value));
    } else {
        put_u8(out, 0xff);
        put_le<std::uint64_t>(out, value);
    }
}

std::array<std::byte, 32> filled_hash(std::uint8_t fill)
{
    std::array<std::byte, 32> hash{};
    hash.fill(std::byte{fill});
    return hash;
}

// Appends an 80-byte block header. `seed` differentiates the field values.
void put_block_header(std::vector<std::byte>& out, std::uint8_t seed)
{
    put_le<std::int32_t>(out, static_cast<std::int32_t>(seed)); // version
    for (int i = 0; i < 32; ++i) {                              // prev block hash
        put_u8(out, seed);
    }
    for (int i = 0; i < 32; ++i) {                             // merkle root
        put_u8(out, static_cast<std::uint8_t>(seed + 1));
    }
    put_le<std::uint32_t>(out, 0x1234'5678);                    // timestamp
    put_le<std::uint32_t>(out, 0x1d00'ffff);                    // bits
    put_le<std::uint32_t>(out, 0xdead'beef);                    // nonce
}

// Appends a full headers-message entry: 80-byte header + zero txn count.
void put_header_entry(std::vector<std::byte>& out, std::uint8_t seed)
{
    put_block_header(out, seed);
    put_compact_size(out, 0); // headers carry no transactions
}

} // namespace

BOOST_AUTO_TEST_CASE(empty_headers_succeeds)
{
    std::vector<std::byte> payload;
    put_compact_size(payload, 0);

    const auto result = parse_headers_payload(payload);

    BOOST_REQUIRE(result.has_value());
    BOOST_TEST(result->empty());
}

BOOST_AUTO_TEST_CASE(single_header_is_parsed)
{
    std::vector<std::byte> payload;
    put_compact_size(payload, 1);
    put_header_entry(payload, 0x07);

    const auto result = parse_headers_payload(payload);

    BOOST_REQUIRE(result.has_value());
    BOOST_REQUIRE(result->size() == 1);

    const auto& header = (*result)[0];
    BOOST_TEST(header.version == 0x07);
    BOOST_TEST((header.previous_block_hash == filled_hash(0x07)));
    BOOST_TEST((header.merkle_root == filled_hash(0x08)));
    BOOST_TEST(header.timestamp == 0x1234'5678u);
    BOOST_TEST(header.bits == 0x1d00'ffffu);
    BOOST_TEST(header.nonce == 0xdead'beefu);
}

BOOST_AUTO_TEST_CASE(multiple_headers_are_parsed_in_order)
{
    std::vector<std::byte> payload;
    put_compact_size(payload, 3);
    put_header_entry(payload, 0x10);
    put_header_entry(payload, 0x20);
    put_header_entry(payload, 0x30);

    const auto result = parse_headers_payload(payload);

    BOOST_REQUIRE(result.has_value());
    BOOST_REQUIRE(result->size() == 3);
    BOOST_TEST((*result)[0].version == 0x10);
    BOOST_TEST((*result)[1].version == 0x20);
    BOOST_TEST((*result)[2].version == 0x30);
    BOOST_TEST(((*result)[2].previous_block_hash == filled_hash(0x30)));
}

BOOST_AUTO_TEST_CASE(empty_payload_is_malformed)
{
    const std::vector<std::byte> payload;

    const auto result = parse_headers_payload(payload);

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::malformed_payload);
}

BOOST_AUTO_TEST_CASE(truncated_header_is_malformed)
{
    std::vector<std::byte> payload;
    put_compact_size(payload, 1);
    put_block_header(payload, 0x00);
    payload.pop_back(); // drop one byte of the 80-byte header

    const auto result = parse_headers_payload(payload);

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::malformed_payload);
}

BOOST_AUTO_TEST_CASE(missing_txn_count_is_malformed)
{
    std::vector<std::byte> payload;
    put_compact_size(payload, 1);
    put_block_header(payload, 0x00); // full header but no txn count byte

    const auto result = parse_headers_payload(payload);

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::malformed_payload);
}

BOOST_AUTO_TEST_CASE(nonzero_txn_count_is_malformed)
{
    std::vector<std::byte> payload;
    put_compact_size(payload, 1);
    put_block_header(payload, 0x00);
    put_compact_size(payload, 1); // headers must declare zero transactions

    const auto result = parse_headers_payload(payload);

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::malformed_payload);
}

BOOST_AUTO_TEST_CASE(fewer_headers_than_declared_is_malformed)
{
    std::vector<std::byte> payload;
    put_compact_size(payload, 2); // claims two, supplies one
    put_header_entry(payload, 0x00);

    const auto result = parse_headers_payload(payload);

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::malformed_payload);
}

BOOST_AUTO_TEST_CASE(trailing_bytes_are_rejected)
{
    std::vector<std::byte> payload;
    put_compact_size(payload, 1);
    put_header_entry(payload, 0x00);
    put_u8(payload, 0x99); // one extra byte beyond the declared entry

    const auto result = parse_headers_payload(payload);

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::trailing_bytes);
}

BOOST_AUTO_TEST_CASE(count_above_maximum_is_rejected)
{
    std::vector<std::byte> payload;
    put_compact_size(payload, 2001); // MAX_HEADERS_RESULTS is 2000

    const auto result = parse_headers_payload(payload);

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::compact_size_too_large);
}

BOOST_AUTO_TEST_CASE(non_canonical_count_is_rejected)
{
    // Encode the value 1 using the three-byte 0xfd form instead of one byte.
    std::vector<std::byte> payload{
        std::byte{0xfd}, std::byte{0x01}, std::byte{0x00}
    };

    const auto result = parse_headers_payload(payload);

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::non_canonical_compact_size);
}
