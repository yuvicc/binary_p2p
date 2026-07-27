#define BOOST_TEST_MODULE RawMessageParser
#include <boost/test/unit_test.hpp>

#include "network_magic.h"
#include "raw_message_parser.h"

#include <array>
#include <cstddef>
#include <ostream>
#include <span>
#include <vector>

namespace boost::test_tools::tt_detail {

template<>
struct print_log_value<std::byte> {
    void operator()(std::ostream& os, std::byte b) const
    {
        os << "0x" << std::hex << std::to_integer<int>(b);
    }
};

template<>
struct print_log_value<ParseError> {
    void operator()(std::ostream& os, ParseError e) const
    {
        os << static_cast<int>(e);
    }
};
} // namespace boost::test_tools::tt_detail

namespace {

constexpr std::array<std::byte, 24> mainnet_verack{
    // Magic
    std::byte{0xf9},
    std::byte{0xbe},
    std::byte{0xb4},
    std::byte{0xd9},

    // Command: "verack"
    std::byte{0x76},
    std::byte{0x65},
    std::byte{0x72},
    std::byte{0x61},
    std::byte{0x63},
    std::byte{0x6b},
    std::byte{0x00},
    std::byte{0x00},
    std::byte{0x00},
    std::byte{0x00},
    std::byte{0x00},
    std::byte{0x00},

    // Payload size: 0
    std::byte{0x00},
    std::byte{0x00},
    std::byte{0x00},
    std::byte{0x00},

    // Empty-payload checksum
    std::byte{0x5d},
    std::byte{0xf6},
    std::byte{0xe0},
    std::byte{0xe2},
};

constexpr std::array<std::byte, 32> mainnet_ping_42{
    // Magic
    std::byte{0xf9},
    std::byte{0xbe},
    std::byte{0xb4},
    std::byte{0xd9},

    // Command: "ping"
    std::byte{0x70},
    std::byte{0x69},
    std::byte{0x6e},
    std::byte{0x67},
    std::byte{0x00},
    std::byte{0x00},
    std::byte{0x00},
    std::byte{0x00},
    std::byte{0x00},
    std::byte{0x00},
    std::byte{0x00},
    std::byte{0x00},

    // Payload size: 8
    std::byte{0x08},
    std::byte{0x00},
    std::byte{0x00},
    std::byte{0x00},

    // Checksum
    std::byte{0xf2},
    std::byte{0x71},
    std::byte{0x62},
    std::byte{0x78},

    // Nonce: 42, little-endian uint64
    std::byte{0x2a},
    std::byte{0x00},
    std::byte{0x00},
    std::byte{0x00},
    std::byte{0x00},
    std::byte{0x00},
    std::byte{0x00},
    std::byte{0x00},
};

} // namespace

BOOST_AUTO_TEST_CASE(empty_verack)
{
    const auto result = parse_raw_message(
        mainnet_verack,
        NetworkMagic::mainnet
    );

    BOOST_REQUIRE(result.has_value());
    BOOST_TEST(result->message.header.command_name() == "verack");
    BOOST_TEST(result->message.header.payload_size == 0);
    BOOST_TEST(result->message.payload.empty());
    BOOST_TEST(result->bytes_consumed == 24);
}

BOOST_AUTO_TEST_CASE(ping_payload)
{
    const auto result = parse_raw_message(
        mainnet_ping_42,
        NetworkMagic::mainnet
    );

    BOOST_REQUIRE(result.has_value());
    BOOST_TEST(result->message.header.command_name() == "ping");
    BOOST_TEST(result->message.header.payload_size == 8);
    BOOST_TEST(result->message.payload.size() == 8);
    BOOST_TEST(result->message.payload[0] == std::byte{0x2a});
    BOOST_TEST(result->bytes_consumed == 32);
}

BOOST_AUTO_TEST_CASE(incomplete_payload)
{
    // The header declares eight payload bytes, but only six are supplied.
    const auto incomplete = std::span{mainnet_ping_42}.first(30);

    const auto result = parse_raw_message(
        incomplete,
        NetworkMagic::mainnet
    );

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::insufficient_data);
}

BOOST_AUTO_TEST_CASE(checksum_mismatch)
{
    auto corrupted = mainnet_ping_42;

    // Mutate one payload byte without changing the header checksum.
    corrupted[24] = std::byte{0x2b};

    const auto result = parse_raw_message(
        corrupted,
        NetworkMagic::mainnet
    );

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::checksum_mismatch);
}

BOOST_AUTO_TEST_CASE(multiple_messages)
{
    std::vector<std::byte> bytes;

    bytes.insert(bytes.end(), mainnet_verack.begin(), mainnet_verack.end());
    bytes.insert(bytes.end(), mainnet_verack.begin(), mainnet_verack.end());

    const auto first = parse_raw_message(bytes, NetworkMagic::mainnet);

    BOOST_REQUIRE(first.has_value());
    BOOST_TEST(first->bytes_consumed == 24);

    const auto remaining = std::span{bytes}.subspan(first->bytes_consumed);

    const auto second = parse_raw_message(remaining, NetworkMagic::mainnet);

    BOOST_REQUIRE(second.has_value());
    BOOST_TEST(second->bytes_consumed == 24);
}
