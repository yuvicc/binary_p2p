#define BOOST_TEST_MODULE MessageStreamDecoder
#include <boost/test/unit_test.hpp>

#include "message_stream_decoder.h"
#include "network_magic.h"

#include <array>
#include <cstddef>
#include <optional>
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

// A complete mainnet "verack" frame (24-byte header, empty payload).
constexpr std::array<std::byte, 24> verack_wire{
    std::byte{0xf9}, std::byte{0xbe}, std::byte{0xb4}, std::byte{0xd9},
    std::byte{0x76}, std::byte{0x65}, std::byte{0x72}, std::byte{0x61},
    std::byte{0x63}, std::byte{0x6b}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x5d}, std::byte{0xf6}, std::byte{0xe0}, std::byte{0xe2},
};

// A complete mainnet "ping" frame (24-byte header + 8-byte nonce payload).
constexpr std::array<std::byte, 32> ping_wire{
    std::byte{0xf9}, std::byte{0xbe}, std::byte{0xb4}, std::byte{0xd9},
    std::byte{0x70}, std::byte{0x69}, std::byte{0x6e}, std::byte{0x67},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x08}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0xf2}, std::byte{0x71}, std::byte{0x62}, std::byte{0x78},
    std::byte{0x2a}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
};

template<std::size_t N>
void append(std::vector<std::byte>& out, const std::array<std::byte, N>& src)
{
    out.insert(out.end(), src.begin(), src.end());
}

} // namespace

BOOST_AUTO_TEST_CASE(single_message_in_one_feed)
{
    MessageStreamDecoder decoder{NetworkMagic::mainnet};
    decoder.feed(verack_wire);

    const auto first = decoder.next();
    BOOST_REQUIRE(first.has_value());
    BOOST_REQUIRE(first->has_value());
    BOOST_TEST((**first).header.command_name() == "verack");

    const auto second = decoder.next();
    BOOST_REQUIRE(second.has_value());
    BOOST_TEST(!second->has_value()); // buffer drained
    BOOST_TEST(decoder.buffered() == 0);
}

BOOST_AUTO_TEST_CASE(need_more_data_on_empty_stream)
{
    MessageStreamDecoder decoder{NetworkMagic::mainnet};

    const auto result = decoder.next();
    BOOST_REQUIRE(result.has_value());
    BOOST_TEST(!result->has_value());
}

// The scenario from the task: a payload-bearing message split so that its
// header straddles two reads and its payload straddles two more, with the
// following message tacked onto the final read.
BOOST_AUTO_TEST_CASE(reassembles_across_fragmented_reads)
{
    // Stream: one ping (32 bytes) immediately followed by one verack (24).
    std::vector<std::byte> stream;
    append(stream, ping_wire);
    append(stream, verack_wire);
    BOOST_REQUIRE(stream.size() == 56);

    MessageStreamDecoder decoder{NetworkMagic::mainnet};

    const auto feed_range = [&](std::size_t from, std::size_t to) {
        decoder.feed(std::span{stream}.subspan(from, to - from));
    };

    // Call 1: only the first 7 header bytes -> nothing complete.
    feed_range(0, 7);
    {
        const auto r = decoder.next();
        BOOST_REQUIRE(r.has_value());
        BOOST_TEST(!r->has_value());
    }

    // Call 2: rest of the header (to byte 24) plus part of the payload (to 28).
    feed_range(7, 28);
    {
        const auto r = decoder.next();
        BOOST_REQUIRE(r.has_value());
        BOOST_TEST(!r->has_value()); // have 28 bytes, ping needs 32
    }

    // Call 3: the rest of the ping payload (to 32) plus the whole next verack.
    feed_range(28, 56);

    const auto ping = decoder.next();
    BOOST_REQUIRE(ping.has_value());
    BOOST_REQUIRE(ping->has_value());
    BOOST_TEST((**ping).header.command_name() == "ping");
    BOOST_REQUIRE((**ping).payload.size() == 8);
    BOOST_TEST((**ping).payload[0] == std::byte{0x2a});

    const auto verack = decoder.next();
    BOOST_REQUIRE(verack.has_value());
    BOOST_REQUIRE(verack->has_value());
    BOOST_TEST((**verack).header.command_name() == "verack");

    const auto drained = decoder.next();
    BOOST_REQUIRE(drained.has_value());
    BOOST_TEST(!drained->has_value());
    BOOST_TEST(decoder.buffered() == 0);
}

BOOST_AUTO_TEST_CASE(multiple_messages_in_one_feed)
{
    std::vector<std::byte> stream;
    append(stream, verack_wire);
    append(stream, verack_wire);
    append(stream, verack_wire);

    MessageStreamDecoder decoder{NetworkMagic::mainnet};
    decoder.feed(stream);

    int count = 0;
    while (true) {
        const auto r = decoder.next();
        BOOST_REQUIRE(r.has_value());
        if (!r->has_value()) {
            break;
        }
        BOOST_TEST((**r).header.command_name() == "verack");
        ++count;
    }

    BOOST_TEST(count == 3);
    BOOST_TEST(decoder.buffered() == 0);
}

BOOST_AUTO_TEST_CASE(byte_at_a_time_delivery)
{
    MessageStreamDecoder decoder{NetworkMagic::mainnet};

    for (std::size_t i = 0; i < ping_wire.size(); ++i) {
        decoder.feed(std::span{ping_wire}.subspan(i, 1));

        const auto r = decoder.next();
        BOOST_REQUIRE(r.has_value());

        if (i + 1 < ping_wire.size()) {
            BOOST_TEST(!r->has_value()); // incomplete until the final byte
        } else {
            BOOST_REQUIRE(r->has_value());
            BOOST_TEST((**r).header.command_name() == "ping");
        }
    }
}

BOOST_AUTO_TEST_CASE(checksum_mismatch_surfaces_error)
{
    auto corrupted = ping_wire;
    corrupted[24] = std::byte{0x2b}; // flip a payload byte, checksum unchanged

    MessageStreamDecoder decoder{NetworkMagic::mainnet};
    decoder.feed(corrupted);

    const auto result = decoder.next();
    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::checksum_mismatch);
}

BOOST_AUTO_TEST_CASE(incorrect_magic_surfaces_error)
{
    auto corrupted = verack_wire;
    corrupted[0] = std::byte{0x00}; // break the network magic

    MessageStreamDecoder decoder{NetworkMagic::mainnet};
    decoder.feed(corrupted);

    const auto result = decoder.next();
    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::incorrect_magic);
}
