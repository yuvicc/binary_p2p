#include "version_parser.h"

#include "util/reader.h"
#include "compact_size.h"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <utility>

std::expected<NetworkAddress, ParseError>
parse_network_address(ByteReader& reader)
{
    const auto services =
        reader.read_u64_le();

    if (!services) {
        return std::unexpected{
            ParseError::malformed_payload
        };
    }

    const auto ip_address =
        reader.read_array<16>();

    if (!ip_address) {
        return std::unexpected{
            ParseError::malformed_payload
        };
    }

    // Bitcoin ports are encoded in network byte order (big-endian).
    const auto port =
        reader.read_u16_be();

    if (!port) {
        return std::unexpected{
            ParseError::malformed_payload
        };
    }

    return NetworkAddress{
        .services = *services,
        .ip_address = *ip_address,
        .port = *port
    };
}

std::expected<VersionMessage, ParseError>
parse_version_payload(
    std::span<const std::byte> payload
)
{
    ByteReader reader{payload};

    const auto protocol_version =
        reader.read_i32_le();

    if (!protocol_version) {
        return std::unexpected{
            ParseError::malformed_payload
        };
    }

    const auto services =
        reader.read_u64_le();

    if (!services) {
        return std::unexpected{
            ParseError::malformed_payload
        };
    }

    const auto timestamp =
        reader.read_i64_le();

    if (!timestamp) {
        return std::unexpected{
            ParseError::malformed_payload
        };
    }

    const auto receiver_address =
        parse_network_address(reader);

    if (!receiver_address) {
        return std::unexpected{
            receiver_address.error()
        };
    }

    const auto sender_address =
        parse_network_address(reader);

    if (!sender_address) {
        return std::unexpected{
            sender_address.error()
        };
    }

    const auto nonce =
        reader.read_u64_le();

    if (!nonce) {
        return std::unexpected{
            ParseError::malformed_payload
        };
    }

    const auto user_agent_length =
        read_compact_size(
            reader,
            VersionMessage::max_user_agent_length
        );

    if (!user_agent_length) {
        // The RawMessage already contains the complete declared payload, so
        // truncated CompactSize data means the payload is malformed rather
        // than waiting for more network data.
        if (
            user_agent_length.error() ==
            ParseError::insufficient_data
        ) {
            return std::unexpected{
                ParseError::malformed_payload
            };
        }

        return std::unexpected{
            user_agent_length.error()
        };
    }

    const auto user_agent_bytes =
        reader.read_bytes(
            static_cast<std::size_t>(
                *user_agent_length
            )
        );

    if (!user_agent_bytes) {
        return std::unexpected{
            ParseError::malformed_payload
        };
    }

    std::string user_agent;
    user_agent.reserve(
        user_agent_bytes->size()
    );

    for (const std::byte byte : *user_agent_bytes) {
        user_agent.push_back(
            static_cast<char>(
                std::to_integer<unsigned char>(byte)
            )
        );
    }

    const auto start_height =
        reader.read_i32_le();

    if (!start_height) {
        return std::unexpected{
            ParseError::malformed_payload
        };
    }

    std::optional<bool> relay;

    if (!reader.empty()) {
        const auto relay_byte =
            reader.read_u8();

        if (!relay_byte) {
            return std::unexpected{
                ParseError::malformed_payload
            };
        }

        // Bitcoin Core treats zero as false and any nonzero byte as true.
        relay = *relay_byte != 0;
    }

    if (!reader.empty()) {
        return std::unexpected{
            ParseError::trailing_bytes
        };
    }

    return VersionMessage{
        .protocol_version = *protocol_version,
        .services = *services,
        .timestamp = *timestamp,
        .receiver_address = *receiver_address,
        .sender_address = *sender_address,
        .nonce = *nonce,
        .user_agent = std::move(user_agent),
        .start_height = *start_height,
        .relay = relay
    };
}
