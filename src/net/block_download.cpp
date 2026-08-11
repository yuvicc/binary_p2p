#include "net/block_download.h"

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

std::expected<BlockMessage, SyncErrorCode>
request_block(
    Peer& peer,
    const std::array<std::byte, 32>& hash,
    std::uint32_t inventory_type
)
{
    GetdataMessage request{
        .inventory = {
            InventoryVector{.type = inventory_type, .hash = hash},
        },
    };

    if (const auto sent = peer.send("getdata", request); !sent) {
        return std::unexpected{to_sync_error(sent.error())};
    }

    while (true) {
        auto raw = peer.receive();
        if (!raw) {
            return std::unexpected{to_sync_error(raw.error())};
        }

        const auto command = raw->header.command_name();

        if (command == "block") {
            auto message = parse_payload(std::move(*raw));
            if (!message) {
                return std::unexpected{SyncErrorCode::protocol_error};
            }

            auto* block = std::get_if<BlockMessage>(&message->payload);
            if (block == nullptr) {
                return std::unexpected{SyncErrorCode::protocol_error};
            }

            return std::move(*block);
        }

        if (command == "notfound") {
            // The peer does not have the block we asked for.
            return std::unexpected{SyncErrorCode::protocol_error};
        }

        if (command == "ping") {
            auto message = parse_payload(std::move(*raw));
            if (message) {
                if (const auto* ping =
                        std::get_if<PingMessage>(&message->payload)) {
                    (void)peer.send("pong", PongMessage{.nonce = ping->nonce});
                }
            }
            continue;
        }

        // Any other message (inv, addr, etc) is irrelevant here.
    }
}
