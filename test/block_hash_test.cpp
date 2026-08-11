#define BOOST_TEST_MODULE BlockHash
#include <boost/test/unit_test.hpp>

#include "block_hash.h"
#include "block_header.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>
#include <string_view>

namespace boost::test_tools::tt_detail {
template<>
struct print_log_value<std::byte> {
    void operator()(std::ostream& os, std::byte b) const
    {
        os << "0x" << std::hex << std::to_integer<int>(b);
    }
};
}

namespace {

// Parse a hash from its displayed (big-endian) hex into internal byte order.
std::array<std::byte, 32> from_display_hex(std::string_view hex)
{
    std::array<std::byte, 32> out{};
    for (std::size_t i = 0; i < 32; ++i) {
        const auto value = static_cast<std::uint8_t>(
            std::stoi(std::string{hex.substr(i * 2, 2)}, nullptr, 16));
        out[31 - i] = static_cast<std::byte>(value);
    }
    return out;
}

} // namespace

// The regtest genesis header must hash to the well-known regtest genesis id.
BOOST_AUTO_TEST_CASE(regtest_genesis)
{
    const BlockHeader genesis{
        .version = 1,
        .previous_block_hash = {},
        .merkle_root = from_display_hex(
            "4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b"),
        .timestamp = 1296688602,
        .bits = 0x207fffff,
        .nonce = 2,
    };

    const auto expected = from_display_hex(
        "0f9188f13cb7b2c71f2a335e3a4fc328bf5beb436012afca590b1a11466e2206");

    BOOST_TEST((block_hash(genesis) == expected));
}

// The mainnet genesis is a stronger check: a real proof-of-work hash.
BOOST_AUTO_TEST_CASE(mainnet_genesis)
{
    const BlockHeader genesis{
        .version = 1,
        .previous_block_hash = {},
        .merkle_root = from_display_hex(
            "4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b"),
        .timestamp = 1231006505,
        .bits = 0x1d00ffff,
        .nonce = 2083236893,
    };

    const auto expected = from_display_hex(
        "000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f");

    BOOST_TEST((block_hash(genesis) == expected));
}

BOOST_AUTO_TEST_CASE(nonce_change_changes_hash)
{
    BlockHeader header{
        .version = 1,
        .previous_block_hash = {},
        .merkle_root = {},
        .timestamp = 1296688602,
        .bits = 0x207fffff,
        .nonce = 2,
    };

    const auto first = block_hash(header);
    header.nonce = 3;
    const auto second = block_hash(header);

    BOOST_TEST((first != second));
}
