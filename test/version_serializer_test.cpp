#define BOOST_TEST_MODULE VersionSerializer
#include <boost/test/unit_test.hpp>

#include "version_message.h"
#include "version_parser.h"
#include "version_serializer.h"
#include "util/writer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

namespace boost::test_tools::tt_detail {

template<>
struct print_log_value<ParseError> {
    void operator()(std::ostream& os, ParseError e) const
    {
        os << static_cast<int>(e);
    }
};
}

namespace {

std::array<std::byte, 16> ipv4_mapped(std::uint8_t a, std::uint8_t b,
                                      std::uint8_t c, std::uint8_t d)
{
    std::array<std::byte, 16> ip{};
    ip[10] = std::byte{0xff};
    ip[11] = std::byte{0xff};
    ip[12] = std::byte{a};
    ip[13] = std::byte{b};
    ip[14] = std::byte{c};
    ip[15] = std::byte{d};
    return ip;
}

VersionMessage sample()
{
    return VersionMessage{
        .protocol_version = 70016,
        .services = 0x0000'0000'0000'0409ULL,
        .timestamp = 0x0000'0000'6512'3456LL,
        .receiver_address = NetworkAddress{
            .services = 0x01,
            .ip_address = ipv4_mapped(127, 0, 0, 1),
            .port = 8333,
        },
        .sender_address = NetworkAddress{
            .services = 0x0409,
            .ip_address = ipv4_mapped(192, 168, 1, 2),
            .port = 18333,
        },
        .nonce = 0x1122'3344'5566'7788ULL,
        .user_agent = "/Satoshi:25.0.0/",
        .start_height = 812345,
        .relay = true,
    };
}

std::vector<std::byte> encode(const VersionMessage& message)
{
    ByteWriter writer;
    serialize_version_payload(writer, message);
    return writer.take();
}

} // namespace

// The core guarantee: serialize then parse recovers the original message.
BOOST_AUTO_TEST_CASE(round_trips_through_parser)
{
    const auto message = sample();

    const auto bytes = encode(message);
    const auto parsed = parse_version_payload(bytes);

    BOOST_REQUIRE(parsed.has_value());
    BOOST_TEST((*parsed == message));
}

BOOST_AUTO_TEST_CASE(round_trips_relay_false)
{
    auto message = sample();
    message.relay = false;

    const auto parsed = parse_version_payload(encode(message));

    BOOST_REQUIRE(parsed.has_value());
    BOOST_TEST(parsed->relay.has_value());
    BOOST_TEST(*parsed->relay == false);
    BOOST_TEST((*parsed == message));
}

// An absent relay field must not emit a trailing byte, so the parser sees it
// as absent too.
BOOST_AUTO_TEST_CASE(round_trips_absent_relay)
{
    auto message = sample();
    message.relay = std::nullopt;

    const auto bytes = encode(message);
    const auto parsed = parse_version_payload(bytes);

    BOOST_REQUIRE(parsed.has_value());
    BOOST_TEST(!parsed->relay.has_value());
    BOOST_TEST((*parsed == message));
}

BOOST_AUTO_TEST_CASE(empty_user_agent)
{
    auto message = sample();
    message.user_agent.clear();

    const auto parsed = parse_version_payload(encode(message));

    BOOST_REQUIRE(parsed.has_value());
    BOOST_TEST(parsed->user_agent.empty());
    BOOST_TEST((*parsed == message));
}
