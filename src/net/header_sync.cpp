#include "net/header_sync.h"

#include "message.h"
#include "payload_parser.h"

#include <utility>
#include <variant>

namespace {

SyncErrorCode to_sync_error(PeerErrorCode code)
{
    switch (code) {
    case PeerErrorCode::connection_closed:
        return SyncErrorCode::connection_closed;
    case PeerErrorCode::io_error:
        return SyncErrorCode::io_error;
    case PeerErrorCode::protocol_error:
        return SyncErrorCode::protocol_error;
    }
    return SyncErrorCode::protocol_error;
}

} // namespace

std::expected<std::vector<BlockHeader>, SyncErrorCode>
request_headers(
    Peer& peer,
    std::uint32_t protocol_version,
    const std::vector<std::array<std::byte, 32>>& locator,
    const std::array<std::byte, 32>& hash_stop
)
{
    GetHeadersMessage request{
        .protocol_version = protocol_version,
        .block_locator_hashes = locator,
        .hash_stop = hash_stop,
    };

    if (const auto sent = peer.send("getheaders", request); !sent) {
        return std::unexpected{to_sync_error(sent.error())};
    }

    while (true) {
        auto raw = peer.receive();
        if (!raw) {
            return std::unexpected{to_sync_error(raw.error())};
        }

        const auto command = raw->header.command_name();

        if (command == "headers") {
            auto message = parse_payload(std::move(*raw));
            if (!message) {
                return std::unexpected{SyncErrorCode::protocol_error};
            }

            auto* headers = std::get_if<HeadersMessage>(&message->payload);
            if (headers == nullptr) {
                return std::unexpected{SyncErrorCode::protocol_error};
            }

            return std::move(headers->headers);
        }

        if (command == "ping") {
            auto message = parse_payload(std::move(*raw));
            if (message) {
                if (const auto* ping =
                        std::get_if<PingMessage>(&message->payload)) {
                    // Keep the connection alive during a long sync.
                    (void)peer.send("pong", PongMessage{.nonce = ping->nonce});
                }
            }
            continue;
        }

        // Any other message (inv, addr, sendcmpct, ...) is irrelevant here.
    }
}
