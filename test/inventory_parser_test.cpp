#define BOOST_TEST_MODULE InventoryParser
#include <boost/test/unit_test.hpp>

#include "inventory_parser.h"

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
} 

namespace {

void put_u8(std::vector<std::byte>& out, std::uint8_t value)
{
    out.push_back(std::byte{value});
}

void put_u32_le(std::vector<std::byte>& out, std::uint32_t value)
{
    for (int i = 0; i < 4; ++i) {
        put_u8(out, static_cast<std::uint8_t>(value >> (8 * i)));
    }
}

// Encodes a canonical CompactSize.
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
        put_u32_le(out, static_cast<std::uint32_t>(value));
    } else {
        put_u8(out, 0xff);
        for (int i = 0; i < 8; ++i) {
            put_u8(out, static_cast<std::uint8_t>(value >> (8 * i)));
        }
    }
}

// A 32-byte hash whose every byte equals `fill`.
std::array<std::byte, 32> filled_hash(std::uint8_t fill)
{
    std::array<std::byte, 32> hash{};
    hash.fill(std::byte{fill});
    return hash;
}

void put_inventory(
    std::vector<std::byte>& out,
    std::uint32_t type,
    std::uint8_t hash_fill
)
{
    put_u32_le(out, type);
    for (int i = 0; i < 32; ++i) {
        put_u8(out, hash_fill);
    }
}

} // namespace

BOOST_AUTO_TEST_CASE(empty_inventory_succeeds)
{
    std::vector<std::byte> payload;
    put_compact_size(payload, 0);

    const auto result = parse_inventory_payload(payload);

    BOOST_REQUIRE(result.has_value());
    BOOST_TEST(result->empty());
}

BOOST_AUTO_TEST_CASE(single_entry_is_parsed)
{
    std::vector<std::byte> payload;
    put_compact_size(payload, 1);
    put_inventory(payload, InventoryType::tx, 0xAB);

    const auto result = parse_inventory_payload(payload);

    BOOST_REQUIRE(result.has_value());
    BOOST_REQUIRE(result->size() == 1);
    BOOST_TEST((*result)[0].type == InventoryType::tx);
    BOOST_TEST(((*result)[0].hash == filled_hash(0xAB)));
}

BOOST_AUTO_TEST_CASE(multiple_entries_are_parsed_in_order)
{
    std::vector<std::byte> payload;
    put_compact_size(payload, 3);
    put_inventory(payload, InventoryType::tx, 0x11);
    put_inventory(payload, InventoryType::block, 0x22);
    put_inventory(payload, InventoryType::witness_tx, 0x33);

    const auto result = parse_inventory_payload(payload);

    BOOST_REQUIRE(result.has_value());
    BOOST_REQUIRE(result->size() == 3);
    BOOST_TEST((*result)[0].type == InventoryType::tx);
    BOOST_TEST((*result)[1].type == InventoryType::block);
    BOOST_TEST((*result)[2].type == InventoryType::witness_tx);
    BOOST_TEST(((*result)[2].hash == filled_hash(0x33)));
}

BOOST_AUTO_TEST_CASE(unknown_type_round_trips)
{
    std::vector<std::byte> payload;
    put_compact_size(payload, 1);
    put_inventory(payload, 0xDEAD'BEEF, 0x44);

    const auto result = parse_inventory_payload(payload);

    BOOST_REQUIRE(result.has_value());
    BOOST_REQUIRE(result->size() == 1);
    BOOST_TEST((*result)[0].type == 0xDEAD'BEEFu);
}

BOOST_AUTO_TEST_CASE(empty_payload_is_malformed)
{
    const std::vector<std::byte> payload;

    const auto result = parse_inventory_payload(payload);

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::malformed_payload);
}

BOOST_AUTO_TEST_CASE(truncated_entry_is_malformed)
{
    std::vector<std::byte> payload;
    put_compact_size(payload, 1);
    put_inventory(payload, InventoryType::tx, 0x00);
    payload.pop_back(); // drop one byte of the hash

    const auto result = parse_inventory_payload(payload);

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::malformed_payload);
}

BOOST_AUTO_TEST_CASE(fewer_entries_than_declared_is_malformed)
{
    std::vector<std::byte> payload;
    put_compact_size(payload, 2); // claims two, supplies one
    put_inventory(payload, InventoryType::tx, 0x00);

    const auto result = parse_inventory_payload(payload);

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::malformed_payload);
}

BOOST_AUTO_TEST_CASE(trailing_bytes_are_rejected)
{
    std::vector<std::byte> payload;
    put_compact_size(payload, 1);
    put_inventory(payload, InventoryType::tx, 0x00);
    put_u8(payload, 0x99); // one extra byte beyond the declared entry

    const auto result = parse_inventory_payload(payload);

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::trailing_bytes);
}

BOOST_AUTO_TEST_CASE(count_above_maximum_is_rejected)
{
    std::vector<std::byte> payload;
    put_compact_size(payload, 50'001); // MAX_INV_SZ is 50000

    const auto result = parse_inventory_payload(payload);

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::compact_size_too_large);
}

BOOST_AUTO_TEST_CASE(non_canonical_count_is_rejected)
{
    // Encode the value 1 using the three-byte 0xfd form instead of one byte.
    std::vector<std::byte> payload{
        std::byte{0xfd}, std::byte{0x01}, std::byte{0x00}
    };

    const auto result = parse_inventory_payload(payload);

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::non_canonical_compact_size);
}
