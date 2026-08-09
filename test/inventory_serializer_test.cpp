#define BOOST_TEST_MODULE InventorySerializer
#include <boost/test/unit_test.hpp>

#include "inventory.h"
#include "inventory_parser.h"
#include "inventory_serializer.h"
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

// A hash whose bytes are all set to `fill`, for easy identification.
std::array<std::byte, 32> hash_of(std::uint8_t fill)
{
    std::array<std::byte, 32> hash{};
    hash.fill(std::byte{fill});
    return hash;
}

std::vector<std::byte> encode(const std::vector<InventoryVector>& inventory)
{
    ByteWriter writer;
    serialize_inventory_payload(writer, inventory);
    return writer.take();
}

} // namespace

BOOST_AUTO_TEST_CASE(empty_inventory)
{
    const auto bytes = encode({});
    BOOST_REQUIRE(bytes.size() == 1);
    BOOST_TEST(bytes[0] == std::byte{0x00}); // CompactSize count of zero
}

BOOST_AUTO_TEST_CASE(single_entry_layout)
{
    const std::vector<InventoryVector> inventory{
        InventoryVector{.type = InventoryType::tx, .hash = hash_of(0xAB)},
    };

    const auto bytes = encode(inventory);

    // 1 (count) + 4 (type) + 32 (hash)
    BOOST_REQUIRE(bytes.size() == 37);
    BOOST_TEST(bytes[0] == std::byte{0x01});
    BOOST_TEST(bytes[1] == std::byte{0x01}); // type = 1, LE
    BOOST_TEST(bytes[2] == std::byte{0x00});
    BOOST_TEST(bytes[5] == std::byte{0xAB}); // first hash byte
}

// The core guarantee: serialize then parse recovers the original inventory.
BOOST_AUTO_TEST_CASE(round_trips_through_parser)
{
    const std::vector<InventoryVector> inventory{
        InventoryVector{.type = InventoryType::tx, .hash = hash_of(0x01)},
        InventoryVector{.type = InventoryType::block, .hash = hash_of(0x02)},
        InventoryVector{.type = InventoryType::witness_tx, .hash = hash_of(0xFF)},
    };

    const auto bytes = encode(inventory);
    const auto parsed = parse_inventory_payload(bytes);

    BOOST_REQUIRE(parsed.has_value());
    BOOST_TEST((*parsed == inventory));
}

BOOST_AUTO_TEST_CASE(round_trips_empty_through_parser)
{
    const std::vector<InventoryVector> inventory{};

    const auto bytes = encode(inventory);
    const auto parsed = parse_inventory_payload(bytes);

    BOOST_REQUIRE(parsed.has_value());
    BOOST_TEST(parsed->empty());
}
