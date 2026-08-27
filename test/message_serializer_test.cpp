#define BOOST_TEST_MODULE MessageSerializer
#include <boost/test/unit_test.hpp>

#include "message.h"
#include "message_header.h"
#include "message_serializer.h"
#include "message_stream_decoder.h"
#include "network_magic.h"
#include "parse_error.h"
#include "payload_parser.h"
#include "raw_message_parser.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ostream>
#include <span>
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

VersionMessage sample_version()
{
    return VersionMessage{
        .protocol_version = 70016,
        .services = 0x0409,
        .timestamp = 0x6512'3456,
        .receiver_address = NetworkAddress{.services = 1, .port = 8333},
        .sender_address = NetworkAddress{.services = 0x0409, .port = 8333},
        .nonce = 0x1122'3344'5566'7788ULL,
        .user_agent = "/Satoshi:25.0.0/",
        .start_height = 812345,
        .relay = true,
    };
}

// Serialize a payload, parse the resulting wire bytes all the way back to a
// typed Message, and return the recovered payload variant.
MessagePayload round_trip(std::string_view command, const MessagePayload& payload)
{
    const auto bytes = serialize_message(NetworkMagic::mainnet, command, payload);

    const auto raw = parse_raw_message(bytes, NetworkMagic::mainnet);
    BOOST_REQUIRE(raw.has_value());
    BOOST_REQUIRE(raw->bytes_consumed == bytes.size());
    BOOST_TEST(raw->message.header.command_name() == command);

    auto message = parse_payload(std::move(raw->message));
    BOOST_REQUIRE(message.has_value());
    return std::move(message->payload);
}

} // namespace

BOOST_AUTO_TEST_CASE(header_framing_layout)
{
    const auto bytes =
        serialize_message(NetworkMagic::mainnet, "ping", PingMessage{.nonce = 42});

    // 24-byte header + 8-byte nonce payload.
    BOOST_REQUIRE(bytes.size() == 32);
    // magic
    BOOST_TEST(bytes[0] == std::byte{0xf9});
    BOOST_TEST(bytes[3] == std::byte{0xd9});
    // command, zero-padded
    BOOST_TEST(bytes[4] == std::byte{'p'});
    BOOST_TEST(bytes[7] == std::byte{'g'});
    BOOST_TEST(bytes[8] == std::byte{0x00});
    // LE payload size == 8
    BOOST_TEST(bytes[16] == std::byte{0x08});
    BOOST_TEST(bytes[17] == std::byte{0x00});
}

BOOST_AUTO_TEST_CASE(round_trip_verack)
{
    const auto recovered = round_trip("verack", VerackMessage{});
    BOOST_TEST(std::holds_alternative<VerackMessage>(recovered));
}

BOOST_AUTO_TEST_CASE(round_trip_ping_pong)
{
    const auto ping = round_trip("ping", PingMessage{.nonce = 0xDEAD'BEEF'CAFEULL});
    BOOST_REQUIRE(std::holds_alternative<PingMessage>(ping));
    BOOST_TEST(std::get<PingMessage>(ping).nonce == 0xDEAD'BEEF'CAFEULL);

    const auto pong = round_trip("pong", PongMessage{.nonce = 7});
    BOOST_REQUIRE(std::holds_alternative<PongMessage>(pong));
    BOOST_TEST(std::get<PongMessage>(pong).nonce == 7U);
}

BOOST_AUTO_TEST_CASE(round_trip_version)
{
    const auto original = sample_version();
    const auto recovered = round_trip("version", original);

    BOOST_REQUIRE(std::holds_alternative<VersionMessage>(recovered));
    BOOST_TEST((std::get<VersionMessage>(recovered) == original));
}

BOOST_AUTO_TEST_CASE(round_trip_inv)
{
    const InvMessage original{
        .inventory = {
            InventoryVector{.type = InventoryType::tx, .hash = hash_of(0x11)},
            InventoryVector{.type = InventoryType::block, .hash = hash_of(0x22)},
        },
    };

    const auto recovered = round_trip("inv", original);
    BOOST_REQUIRE(std::holds_alternative<InvMessage>(recovered));
    BOOST_TEST((std::get<InvMessage>(recovered) == original));
}

BOOST_AUTO_TEST_CASE(round_trip_addr)
{
    std::array<std::byte, 16> ip{};
    ip[10] = std::byte{0xff};
    ip[11] = std::byte{0xff};
    ip[12] = std::byte{203};
    ip[15] = std::byte{7};

    const AddrMessage original{
        .addresses = {
            AddressEntry{
                .timestamp = 1'700'000'000U,
                .address = NetworkAddress{
                    .services = 0x0409,
                    .ip_address = ip,
                    .port = 8333,
                },
            },
            AddressEntry{
                .timestamp = 1'700'000'001U,
                .address = NetworkAddress{.services = 1, .port = 18333},
            },
        },
    };

    const auto recovered = round_trip("addr", original);
    BOOST_REQUIRE(std::holds_alternative<AddrMessage>(recovered));
    BOOST_TEST((std::get<AddrMessage>(recovered) == original));
}

BOOST_AUTO_TEST_CASE(round_trip_empty_addr)
{
    const auto recovered = round_trip("addr", AddrMessage{});
    BOOST_REQUIRE(std::holds_alternative<AddrMessage>(recovered));
    BOOST_TEST(std::get<AddrMessage>(recovered).addresses.empty());
}

BOOST_AUTO_TEST_CASE(round_trip_getaddr)
{
    const auto recovered = round_trip("getaddr", GetAddrMessage{});
    BOOST_TEST(std::holds_alternative<GetAddrMessage>(recovered));
}

BOOST_AUTO_TEST_CASE(round_trip_headers)
{
    const HeadersMessage original{
        .headers = {
            BlockHeader{
                .version = 0x2000'0000,
                .previous_block_hash = hash_of(0x01),
                .merkle_root = hash_of(0x02),
                .timestamp = 0x5f5e'0100U,
                .bits = 0x1707'1e2bU,
                .nonce = 0xDEAD'BEEFU,
            },
        },
    };

    const auto recovered = round_trip("headers", original);
    BOOST_REQUIRE(std::holds_alternative<HeadersMessage>(recovered));
    BOOST_TEST((std::get<HeadersMessage>(recovered) == original));
}

// serialize_message(const Message&) takes magic and command from the header.
BOOST_AUTO_TEST_CASE(serialize_from_message_header)
{
    Message message{
        .header = MessageHeader{"ping", 0},
        .payload = MessagePayload{PingMessage{.nonce = 99}},
    };
    message.header.magic = NetworkMagic::mainnet;

    const auto bytes = serialize_message(message);

    const auto raw = parse_raw_message(bytes, NetworkMagic::mainnet);
    BOOST_REQUIRE(raw.has_value());
    BOOST_TEST(raw->message.header.command_name() == "ping");

    auto parsed = parse_payload(std::move(raw->message));
    BOOST_REQUIRE(parsed.has_value());
    BOOST_REQUIRE(std::holds_alternative<PingMessage>(parsed->payload));
    BOOST_TEST(std::get<PingMessage>(parsed->payload).nonce == 99U);
}

// Two serialized messages, concatenated, must decode cleanly through the stream
// decoder -- proving the encoder produces exactly what the framing layer reads.
BOOST_AUTO_TEST_CASE(serialized_messages_flow_through_stream_decoder)
{
    const auto first =
        serialize_message(NetworkMagic::mainnet, "ping", PingMessage{.nonce = 1});
    const auto second = serialize_message(NetworkMagic::mainnet, "verack", VerackMessage{});

    std::vector<std::byte> stream;
    stream.insert(stream.end(), first.begin(), first.end());
    stream.insert(stream.end(), second.begin(), second.end());

    MessageStreamDecoder decoder{NetworkMagic::mainnet};
    decoder.feed(stream);

    const auto a = decoder.next();
    BOOST_REQUIRE(a.has_value());
    BOOST_REQUIRE(a->has_value());
    BOOST_TEST((**a).header.command_name() == "ping");

    const auto b = decoder.next();
    BOOST_REQUIRE(b.has_value());
    BOOST_REQUIRE(b->has_value());
    BOOST_TEST((**b).header.command_name() == "verack");

    const auto drained = decoder.next();
    BOOST_REQUIRE(drained.has_value());
    BOOST_TEST(!drained->has_value());
    BOOST_TEST(decoder.buffered() == 0);
}
