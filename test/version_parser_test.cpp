#define BOOST_TEST_MODULE VersionParser
#include <boost/test/unit_test.hpp>

#include "version_parser.h"
#include "version_payload_fixture.h"

#include <cstddef>
#include <optional>
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
} // namespace boost::test_tools::tt_detail

using namespace test_support;

BOOST_AUTO_TEST_CASE(valid_version_present_relay)
{
    const auto payload = build_version_payload(1);
    const auto result = parse_version_payload(payload);

    BOOST_REQUIRE(result.has_value());
    BOOST_TEST(result->protocol_version == 70015);
    BOOST_TEST(result->services == 1u);
    BOOST_TEST(result->timestamp == 1231006505);
    BOOST_TEST(result->receiver_address.services == 1u);
    BOOST_TEST(result->receiver_address.port == 8333);
    BOOST_TEST(result->sender_address.services == 0u);
    BOOST_TEST(result->nonce == 0x1122334455667788ULL);
    BOOST_TEST(result->user_agent == "/Satoshi:1.0/");
    BOOST_TEST(result->start_height == 700000);
    BOOST_REQUIRE(result->relay.has_value());
    BOOST_TEST(result->relay.value() == true);
    BOOST_TEST(result->relays_transactions() == true);
}

BOOST_AUTO_TEST_CASE(absent_relay_defaults_to_true)
{
    const auto payload = build_version_payload(-1);
    const auto result = parse_version_payload(payload);

    BOOST_REQUIRE(result.has_value());
    BOOST_TEST(!result->relay.has_value());
    BOOST_TEST(result->relays_transactions() == true);
}

BOOST_AUTO_TEST_CASE(relay_false_is_preserved)
{
    const auto payload = build_version_payload(0);
    const auto result = parse_version_payload(payload);

    BOOST_REQUIRE(result.has_value());
    BOOST_REQUIRE(result->relay.has_value());
    BOOST_TEST(result->relay.value() == false);
    BOOST_TEST(result->relays_transactions() == false);
}

BOOST_AUTO_TEST_CASE(truncated_payload_is_malformed)
{
    auto payload = build_version_payload(1);
    payload.resize(10); // cut off partway through the fixed header

    const auto result = parse_version_payload(payload);

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::malformed_payload);
}

BOOST_AUTO_TEST_CASE(trailing_bytes_are_rejected)
{
    auto payload = build_version_payload(1);
    payload.push_back(std::byte{0x00}); // extra byte after the relay flag

    const auto result = parse_version_payload(payload);

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::trailing_bytes);
}

BOOST_AUTO_TEST_CASE(oversized_user_agent_is_rejected)
{
    // Everything up to the user-agent length prefix, then claim 300 bytes.
    std::vector<std::byte> payload;
    put_le<std::int32_t>(payload, 70015);
    put_le<std::uint64_t>(payload, 1);
    put_le<std::int64_t>(payload, 1231006505);
    put_network_address(payload, 1, 8333);
    put_network_address(payload, 0, 0);
    put_le<std::uint64_t>(payload, 0ULL);

    // CompactSize 0xfd 0x2c 0x01 == 300, above max_user_agent_length (256).
    put_u8(payload, 0xfd);
    put_u8(payload, 0x2c);
    put_u8(payload, 0x01);

    const auto result = parse_version_payload(payload);

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::compact_size_too_large);
}
