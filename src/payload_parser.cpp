#include "payload_parser.h"

#include "headers_parser.h"
#include "inventory_parser.h"
#include "util/reader.h"
#include "version_parser.h"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <utility>

namespace {

[[nodiscard]]
std::expected<std::uint64_t, ParseError>
parse_nonce_payload(
    std::span<const std::byte> payload
)
{
    ByteReader reader{payload};

    const auto nonce = reader.read_u64_le();

    if (!nonce) {
        // The raw-message parser already collected the complete declared
        // payload, so insufficient bytes here mean the command payload is
        // malformed.
        return std::unexpected{
            ParseError::malformed_payload
        };
    }

    // A modern ping or pong contains exactly one uint64_t; anything left over
    // is not part of the expected format.
    if (!reader.empty()) {
        return std::unexpected{
            ParseError::trailing_bytes
        };
    }

    return *nonce;
}

} // namespace

std::expected<Message, ParseError>
parse_payload(RawMessage raw_message)
{
    // The raw-message parser should already guarantee this invariant; keeping
    // the check makes this function safe for manually constructed RawMessages.
    if (
        static_cast<std::size_t>(
            raw_message.header.payload_size
        ) != raw_message.payload.size()
    ) {
        return std::unexpected{
            ParseError::malformed_payload
        };
    }

    const std::string_view command =
        raw_message.header.command_name();

    if (command == "version") {
        const auto version =
            parse_version_payload(
                std::span<const std::byte>{
                    raw_message.payload
                }
            );

        if (!version) {
            return std::unexpected{
                version.error()
            };
        }

        return Message{
            .header = std::move(
                raw_message.header
            ),
            .payload = MessagePayload{
                std::move(*version)
            }
        };
    }

    if (command == "verack") {
        if (!raw_message.payload.empty()) {
            return std::unexpected{
                ParseError::trailing_bytes
            };
        }

        return Message{
            .header = std::move(raw_message.header),
            .payload = MessagePayload{
                VerackMessage{}
            },
        };
    }

    if (command == "ping") {
        const auto nonce = parse_nonce_payload(
            raw_message.payload
        );

        if (!nonce) {
            return std::unexpected{
                nonce.error()
            };
        }

        return Message{
            .header = std::move(raw_message.header),
            .payload = MessagePayload{
                PingMessage{
                    .nonce = *nonce,
                }
            },
        };
    }

    if (command == "pong") {
        const auto nonce = parse_nonce_payload(
            raw_message.payload
        );

        if (!nonce) {
            return std::unexpected{
                nonce.error()
            };
        }

        return Message{
            .header = std::move(raw_message.header),
            .payload = MessagePayload{
                PongMessage{
                    .nonce = *nonce,
                }
            },
        };
    }

    if (command == "inv") {
        auto inventory = parse_inventory_payload(
            raw_message.payload
        );

        if (!inventory) {
            return std::unexpected{
                inventory.error()
            };
        }

        return Message{
            .header = std::move(raw_message.header),
            .payload = MessagePayload{
                InvMessage{
                    .inventory = std::move(*inventory),
                }
            },
        };
    }

    if (command == "getdata") {
        auto inventory = parse_inventory_payload(
            raw_message.payload
        );

        if (!inventory) {
            return std::unexpected{
                inventory.error()
            };
        }

        return Message{
            .header = std::move(raw_message.header),
            .payload = MessagePayload{
                GetdataMessage{
                    .inventory = std::move(*inventory),
                }
            },
        };
    }

    if (command == "headers") {
        auto headers = parse_headers_payload(
            raw_message.payload
        );

        if (!headers) {
            return std::unexpected{
                headers.error()
            };
        }

        return Message{
            .header = std::move(raw_message.header),
            .payload = MessagePayload{
                HeadersMessage{
                    .headers = std::move(*headers),
                }
            },
        };
    }

    // An unknown command is not automatically malformed: a newer peer may send
    // a command this parser does not yet understand. Preserve the raw payload
    // so higher-level code can inspect, ignore, log, or process it later.
    return Message{
        .header = std::move(raw_message.header),
        .payload = MessagePayload{
            UnknownMessage{
                .payload = std::move(
                    raw_message.payload
                ),
            }
        },
    };
}
