#define BOOST_TEST_MODULE GetHeaders
#include <boost/test/unit_test.hpp>

#include "getheaders_parser.h"
#include "getheaders_serializer.h"
#include "message.h"
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

std::vector<std::byte> encode(const GetHeadersMessage& message)
{
    ByteWriter writer;
    serialize_getheaders_payload(writer, message);
    return writer.take();
}

} // namespace

BOOST_AUTO_TEST_CASE(layout_with_two_locators)
{
    const GetHeadersMessage message{
        .protocol_version = 70016,
        .block_locator_hashes = {hash_of(0x01), hash_of(0x02)},
        .hash_stop = hash_of(0x00),
    };

    const auto bytes = encode(message);

    // 4 (version) + 1 (count) + 2*32 (locators) + 32 (hash_stop)
    BOOST_REQUIRE(bytes.size() == 4 + 1 + 64 + 32);
    BOOST_TEST(bytes[4] == std::byte{0x02}); // locator count
}

BOOST_AUTO_TEST_CASE(round_trips_through_parser)
{
    const GetHeadersMessage message{
        .protocol_version = 70016,
        .block_locator_hashes = {hash_of(0x11), hash_of(0x22), hash_of(0x33)},
        .hash_stop = hash_of(0xFF),
    };

    const auto parsed = parse_getheaders_payload(encode(message));

    BOOST_REQUIRE(parsed.has_value());
    BOOST_TEST((*parsed == message));
}

// An empty locator (just version + zero count + hash_stop) is valid.
BOOST_AUTO_TEST_CASE(round_trips_empty_locator)
{
    const GetHeadersMessage message{
        .protocol_version = 70016,
        .block_locator_hashes = {},
        .hash_stop = hash_of(0xAB),
    };

    const auto parsed = parse_getheaders_payload(encode(message));

    BOOST_REQUIRE(parsed.has_value());
    BOOST_TEST(parsed->block_locator_hashes.empty());
    BOOST_TEST((*parsed == message));
}

BOOST_AUTO_TEST_CASE(truncated_payload_is_malformed)
{
    // Version present but the locator/hash_stop are missing.
    const std::vector<std::byte> bytes{
        std::byte{0x01}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    };

    const auto parsed = parse_getheaders_payload(bytes);

    BOOST_REQUIRE(!parsed.has_value());
    BOOST_TEST(parsed.error() == ParseError::malformed_payload);
}
