#define BOOST_TEST_MODULE MessageHeaderParser
#include <boost/test/unit_test.hpp>

#include "message_header_parser.h"
#include "network_magic.h"

#include <array>
#include <cstddef>
#include <ostream>


namespace boost::test_tools::tt_detail {

template<>
struct print_log_value<std::byte> {
    void operator()(std::ostream& os, std::byte b) const
    {
        os << "0x" << std::hex << std::to_integer<int>(b);
    }
};

template<>
struct print_log_value<HeaderParseErrorCode> {
    void operator()(std::ostream& os, HeaderParseErrorCode c) const
    {
        os << static_cast<int>(c);
    }
};
} // namespace boost::test_tools::tt_detail

BOOST_AUTO_TEST_CASE(valid_mainnet_verack_header)
{
    const std::array<std::byte, 24> bytes{
        // Mainnet magic
        std::byte{0xf9}, std::byte{0xbe}, std::byte{0xb4}, std::byte{0xd9},

        // "verack" followed by zero padding
        std::byte{0x76}, std::byte{0x65}, std::byte{0x72}, std::byte{0x61},
        std::byte{0x63}, std::byte{0x6b}, std::byte{0x00}, std::byte{0x00},
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},

        // Payload size: zero
        std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},

        // Empty-payload checksum
        std::byte{0x5d}, std::byte{0xf6}, std::byte{0xe0}, std::byte{0xe2},
    };

    const auto result = parse_message_header(bytes, NetworkMagic::mainnet);

    BOOST_REQUIRE(result.has_value());
    BOOST_TEST(result->magic == NetworkMagic::mainnet);
    BOOST_TEST(result->command_name() == "verack");
    BOOST_TEST(result->payload_size == 0);

    BOOST_TEST(result->checksum[0] == std::byte{0x5d});
    BOOST_TEST(result->checksum[1] == std::byte{0xf6});
    BOOST_TEST(result->checksum[2] == std::byte{0xe0});
    BOOST_TEST(result->checksum[3] == std::byte{0xe2});
}

BOOST_AUTO_TEST_CASE(insufficient_data)
{
    const std::array<std::byte, 23> bytes{};

    const auto result = parse_message_header(bytes, NetworkMagic::mainnet);

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error().code == HeaderParseErrorCode::insufficient_data);
}

BOOST_AUTO_TEST_CASE(incorrect_magic)
{
    std::array<std::byte, 24> bytes{};

    bytes[0] = std::byte{0xfa};
    bytes[1] = std::byte{0xbf};
    bytes[2] = std::byte{0xb5};
    bytes[3] = std::byte{0xda};

    // Create a valid command so magic is the relevant failure.
    bytes[4] = std::byte{0x70}; // p
    bytes[5] = std::byte{0x69}; // i
    bytes[6] = std::byte{0x6e}; // n
    bytes[7] = std::byte{0x67}; // g

    const auto result = parse_message_header(bytes, NetworkMagic::mainnet);

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error().code == HeaderParseErrorCode::incorrect_magic);
}

BOOST_AUTO_TEST_CASE(invalid_command_padding)
{
    std::array<std::byte, 24> bytes{};

    bytes[0] = std::byte{0xf9};
    bytes[1] = std::byte{0xbe};
    bytes[2] = std::byte{0xb4};
    bytes[3] = std::byte{0xd9};

    bytes[4] = std::byte{0x70}; // p
    bytes[5] = std::byte{0x69}; // i
    bytes[6] = std::byte{0x00}; // padding starts
    bytes[7] = std::byte{0x67}; // invalid nonzero byte after padding

    const auto result = parse_message_header(bytes, NetworkMagic::mainnet);

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error().code == HeaderParseErrorCode::invalid_command);
}
