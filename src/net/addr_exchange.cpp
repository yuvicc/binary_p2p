#include "net/addr_exchange.h"

#include "message.h"
#include "payload_parser.h"

#include <algorithm>
#include <iterator>
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

std::expected<std::vector<AddressV2Entry>, SyncErrorCode>
request_addresses(Peer& peer)
{
    if (const auto sent = peer.send("getaddr", GetAddrMessage{}); !sent) {
        return std::unexpected{to_sync_error(sent.error())};
    }

    while (true) {
        auto raw = peer.receive();
        if (!raw) {
            return std::unexpected{to_sync_error(raw.error())};
        }

        const auto command = raw->header.command_name();

        if (command == "addrv2") {
            auto message = parse_payload(std::move(*raw));
            if (!message) {
                return std::unexpected{SyncErrorCode::protocol_error};
            }

            auto* addresses = std::get_if<AddrV2Message>(&message->payload);
            if (addresses == nullptr) {
                return std::unexpected{SyncErrorCode::protocol_error};
            }

            return std::move(addresses->addresses);
        }

        if (command == "addr") {
            auto message = parse_payload(std::move(*raw));
            if (!message) {
                return std::unexpected{SyncErrorCode::protocol_error};
            }

            const auto* addresses = std::get_if<AddrMessage>(&message->payload);
            if (addresses == nullptr) {
                return std::unexpected{SyncErrorCode::protocol_error};
            }

            std::vector<AddressV2Entry> converted;
            converted.reserve(addresses->addresses.size());
            std::ranges::transform(
                addresses->addresses,
                std::back_inserter(converted),
                to_addressv2
            );

            return converted;
        }

        if (command == "ping") {
            auto message = parse_payload(std::move(*raw));
            if (message) {
                if (const auto* ping =
                        std::get_if<PingMessage>(&message->payload)) {
                    // Keep the connection alive while the peer decides.
                    (void)peer.send("pong", PongMessage{.nonce = ping->nonce});
                }
            }
            continue;
        }

        // Any other message (inv, headers, sendcmpct, ...) is irrelevant here.
    }
}
