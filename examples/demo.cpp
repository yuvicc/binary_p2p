// A tour of binary_p2p: encoding messages to the Bitcoin wire format, parsing
// them back, and reassembling a fragmented byte stream. Each section is
// self-contained and prints what it did.

#include "compact_size.h"
#include "message.h"
#include "message_serializer.h"
#include "message_stream_decoder.h"
#include "network_magic.h"
#include "payload_parser.h"
#include "raw_message_parser.h"
#include "util/reader.h"
#include "util/writer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {

void rule(std::string_view title)
{
    std::cout << "\n== " << title << " ==\n";
}

// Render bytes as a compact hex string for display.
std::string to_hex(std::span<const std::byte> bytes)
{
    std::string out;
    out.reserve(bytes.size() * 2);
    for (const std::byte byte : bytes) {
        const char digits[] = "0123456789abcdef";
        const auto value = std::to_integer<unsigned>(byte);
        out.push_back(digits[value >> 4]);
        out.push_back(digits[value & 0x0f]);
    }
    return out;
}

std::array<std::byte, 32> hash_filled(std::uint8_t fill)
{
    std::array<std::byte, 32> hash{};
    hash.fill(std::byte{fill});
    return hash;
}

// ---------------------------------------------------------------------------

// The lowest layer: ByteWriter builds a buffer, ByteReader reads it back.
void demo_primitives()
{
    rule("ByteWriter / ByteReader");

    ByteWriter writer;
    writer.write_u8(0x2a);
    writer.write_u32_le(0xdead'beef);
    writer.write_u16_be(8333); // ports are big-endian on the wire

    const auto bytes = writer.take();
    std::cout << "encoded : " << to_hex(bytes) << '\n';

    ByteReader reader{bytes};
    std::cout << "u8      : "
              << static_cast<unsigned>(reader.read_u8().value()) << '\n';
    std::cout << "u32 le  : " << std::hex << reader.read_u32_le().value()
              << std::dec << '\n';
    std::cout << "u16 be  : " << reader.read_u16_be().value() << '\n';
}

// CompactSize is Bitcoin's variable-length integer for counts and lengths.
void demo_compact_size()
{
    rule("CompactSize (varint)");

    for (const std::uint64_t value : {0ULL, 0xfcULL, 0xfdULL, 0x1'0000ULL,
                                      0x1'0000'0000ULL}) {
        ByteWriter writer;
        write_compact_size(writer, value);
        const auto encoded = writer.take();

        ByteReader reader{encoded};
        const auto decoded = read_compact_size(reader).value();

        std::cout << std::setw(12) << value << "  ->  " << std::setw(18)
                  << to_hex(encoded) << "  ->  " << decoded << '\n';
    }
}

// Build a typed message, serialize a full wire frame, and parse it back.
void demo_message_round_trip()
{
    rule("Message encode -> wire -> decode");

    VersionMessage version{
        .protocol_version = 70016,
        .services = 0x0409,
        .timestamp = 1'700'000'000,
        .receiver_address = NetworkAddress{.services = 1, .port = 8333},
        .sender_address = NetworkAddress{.services = 0x0409, .port = 8333},
        .nonce = 0x1122'3344'5566'7788ULL,
        .user_agent = "/binary_p2p:0.1.0/",
        .start_height = 812'345,
        .relay = true,
    };

    // Serialize a complete frame: 24-byte header + payload.
    const auto wire =
        serialize_message(NetworkMagic::mainnet, "version", version);
    std::cout << "frame   : " << wire.size() << " bytes\n";
    std::cout << "header  : " << to_hex(std::span{wire}.first(24)) << '\n';

    // Parse the framing (verifies magic + checksum), then the typed payload.
    const auto raw = parse_raw_message(wire, NetworkMagic::mainnet).value();
    const auto message = parse_payload(raw.message).value();

    const auto& parsed = std::get<VersionMessage>(message.payload);
    std::cout << "command : " << message.header.command_name() << '\n';
    std::cout << "agent   : " << parsed.user_agent << '\n';
    std::cout << "height  : " << parsed.start_height << '\n';
    std::cout << "match   : " << std::boolalpha << (parsed == version) << '\n';
}

// inv and headers carry variable-length collections; both round-trip.
void demo_collections()
{
    rule("inv and headers payloads");

    const InvMessage inv{
        .inventory = {
            InventoryVector{.type = InventoryType::tx, .hash = hash_filled(0x11)},
            InventoryVector{.type = InventoryType::block,
                            .hash = hash_filled(0x22)},
        },
    };

    const auto inv_wire = serialize_message(NetworkMagic::mainnet, "inv", inv);
    const auto inv_back = parse_payload(
        parse_raw_message(inv_wire, NetworkMagic::mainnet).value().message);
    std::cout << "inv items decoded : "
              << std::get<InvMessage>(inv_back.value().payload).inventory.size()
              << '\n';

    const HeadersMessage headers{
        .headers = {
            BlockHeader{
                .version = 0x2000'0000,
                .previous_block_hash = hash_filled(0x01),
                .merkle_root = hash_filled(0x02),
                .timestamp = 1'700'000'000,
                .bits = 0x1707'1e2b,
                .nonce = 0xdead'beef,
            },
        },
    };

    const auto hdr_wire =
        serialize_message(NetworkMagic::mainnet, "headers", headers);
    const auto hdr_back = parse_payload(
        parse_raw_message(hdr_wire, NetworkMagic::mainnet).value().message);
    std::cout << "headers decoded   : "
              << std::get<HeadersMessage>(hdr_back.value().payload).headers.size()
              << '\n';
}

// TCP delivers bytes in arbitrary chunks; the stream decoder reassembles them
// into whole messages. Here two frames arrive split across three reads.
void demo_stream_decoder()
{
    rule("Streaming reassembly across fragmented reads");

    const auto ping =
        serialize_message(NetworkMagic::mainnet, "ping", PingMessage{.nonce = 7});
    const auto verack =
        serialize_message(NetworkMagic::mainnet, "verack", VerackMessage{});

    std::vector<std::byte> stream;
    stream.insert(stream.end(), ping.begin(), ping.end());
    stream.insert(stream.end(), verack.begin(), verack.end());

    MessageStreamDecoder decoder{NetworkMagic::mainnet};

    // Three arbitrary fragment boundaries mid-header and mid-stream.
    const std::size_t cuts[] = {5, 30, stream.size()};
    std::size_t from = 0;

    for (const std::size_t to : cuts) {
        decoder.feed(std::span{stream}.subspan(from, to - from));
        from = to;
        std::cout << "fed up to byte " << to << ", buffered "
                  << decoder.buffered() << '\n';

        while (true) {
            const auto result = decoder.next();
            if (!result) {
                std::cout << "  protocol error\n";
                break;
            }
            if (!result->has_value()) {
                break; // need more bytes
            }
            std::cout << "  decoded message: "
                      << (*result)->header.command_name() << '\n';
        }
    }
}

// A corrupted payload fails the checksum; the decoder surfaces it as an error
// rather than a message.
void demo_error_handling()
{
    rule("Checksum verification");

    auto wire =
        serialize_message(NetworkMagic::mainnet, "ping", PingMessage{.nonce = 1});
    wire.back() ^= std::byte{0xff}; // flip a payload byte

    MessageStreamDecoder decoder{NetworkMagic::mainnet};
    decoder.feed(wire);

    const auto result = decoder.next();
    if (!result) {
        std::cout << "rejected corrupted frame (error code "
                  << static_cast<int>(result.error()) << ")\n";
    } else {
        std::cout << "unexpectedly accepted\n";
    }
}

} // namespace

int main()
{
    std::cout << "binary_p2p demo\n";
    demo_primitives();
    demo_compact_size();
    demo_message_round_trip();
    demo_collections();
    demo_stream_decoder();
    demo_error_handling();
    std::cout << "\ndone.\n";
}
