#define BOOST_TEST_MODULE PayloadParser
#include <boost/test/unit_test.hpp>

#include "payload_parser.h"
#include "version_payload_fixture.h"

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace boost::test_tools::tt_detail {

template<>
struct print_log_value<ParseError> {
    void operator()(std::ostream& os, ParseError e) const
    {
        os << static_cast<int>(e);
    }
};
} // namespace boost::test_tools::tt_detail

using namespace test_support;

namespace {

// Builds a RawMessage whose header payload_size matches the payload length.
RawMessage make_raw(std::string_view command, std::vector<std::byte> payload)
{
    MessageHeader header{
        command,
        static_cast<std::uint32_t>(payload.size())
    };
    return RawMessage{
        .header = header,
        .payload = std::move(payload),
    };
}

std::vector<std::byte> nonce_bytes(std::uint64_t nonce)
{
    std::vector<std::byte> out;
    put_le<std::uint64_t>(out, nonce);
    return out;
}

} // namespace

BOOST_AUTO_TEST_CASE(empty_verack_succeeds)
{
    const auto result = parse_payload(make_raw("verack", {}));

    BOOST_REQUIRE(result.has_value());
    BOOST_TEST(std::holds_alternative<VerackMessage>(result->payload));
    BOOST_TEST(result->header.command_name() == "verack");
}

BOOST_AUTO_TEST_CASE(nonempty_verack_returns_trailing_bytes)
{
    const auto result =
        parse_payload(make_raw("verack", {std::byte{0x00}}));

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::trailing_bytes);
}

BOOST_AUTO_TEST_CASE(eight_byte_ping_succeeds)
{
    const auto result =
        parse_payload(make_raw("ping", nonce_bytes(0xAABBCCDDULL)));

    BOOST_REQUIRE(result.has_value());
    BOOST_REQUIRE(std::holds_alternative<PingMessage>(result->payload));
    BOOST_TEST(std::get<PingMessage>(result->payload).nonce == 0xAABBCCDDULL);
}

BOOST_AUTO_TEST_CASE(short_ping_returns_malformed_payload)
{
    const auto result = parse_payload(
        make_raw("ping", {std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}})
    );

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::malformed_payload);
}

BOOST_AUTO_TEST_CASE(long_ping_returns_trailing_bytes)
{
    auto payload = nonce_bytes(1);
    payload.resize(12); // 8-byte nonce plus 4 unexpected bytes

    const auto result = parse_payload(make_raw("ping", std::move(payload)));

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::trailing_bytes);
}

BOOST_AUTO_TEST_CASE(eight_byte_pong_succeeds)
{
    const auto result =
        parse_payload(make_raw("pong", nonce_bytes(0x99ULL)));

    BOOST_REQUIRE(result.has_value());
    BOOST_REQUIRE(std::holds_alternative<PongMessage>(result->payload));
    BOOST_TEST(std::get<PongMessage>(result->payload).nonce == 0x99ULL);
}

BOOST_AUTO_TEST_CASE(short_pong_returns_malformed_payload)
{
    const auto result = parse_payload(
        make_raw("pong", {std::byte{0}, std::byte{0}})
    );

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::malformed_payload);
}

BOOST_AUTO_TEST_CASE(long_pong_returns_trailing_bytes)
{
    auto payload = nonce_bytes(1);
    payload.resize(16);

    const auto result = parse_payload(make_raw("pong", std::move(payload)));

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::trailing_bytes);
}

BOOST_AUTO_TEST_CASE(unknown_command_preserves_payload)
{
    std::vector<std::byte> payload{
        std::byte{0x01}, std::byte{0x02}, std::byte{0x03}
    };
    const auto expected = payload;

    const auto result = parse_payload(make_raw("addr", std::move(payload)));

    BOOST_REQUIRE(result.has_value());
    BOOST_REQUIRE(std::holds_alternative<UnknownMessage>(result->payload));
    BOOST_TEST((std::get<UnknownMessage>(result->payload).payload == expected));
}

BOOST_AUTO_TEST_CASE(inconsistent_raw_message_is_rejected)
{
    // Header claims eight payload bytes, but only four are present.
    MessageHeader header{"ping", 8};
    RawMessage raw{
        .header = header,
        .payload = {std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}},
    };

    const auto result = parse_payload(std::move(raw));

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::malformed_payload);
}

BOOST_AUTO_TEST_CASE(version_command_is_parsed)
{
    const auto result =
        parse_payload(make_raw("version", build_version_payload(1)));

    BOOST_REQUIRE(result.has_value());
    BOOST_REQUIRE(std::holds_alternative<VersionMessage>(result->payload));

    const auto& version = std::get<VersionMessage>(result->payload);
    BOOST_TEST(version.protocol_version == 70015);
    BOOST_TEST(version.user_agent == "/Satoshi:1.0/");
    BOOST_TEST(version.relays_transactions() == true);
}
