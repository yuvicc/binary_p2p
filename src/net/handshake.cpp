#include "net/handshake.h"

#include "message.h"
#include "payload_parser.h"

#include <utility>
#include <variant>

namespace {

HandshakeErrorCode to_handshake_error(PeerErrorCode code)
{
    switch (code) {
    case PeerErrorCode::connection_closed:
        return HandshakeErrorCode::connection_closed;
    case PeerErrorCode::io_error:
        return HandshakeErrorCode::io_error;
    case PeerErrorCode::protocol_error:
        return HandshakeErrorCode::protocol_error;
    }
    return HandshakeErrorCode::protocol_error;
}

} // namespace

std::expected<HandshakeResult, HandshakeErrorCode>
perform_handshake(Peer& peer, const VersionMessage& local_version)
{
    // 1. Announce ourselves.
    if (const auto sent = peer.send("version", local_version); !sent) {
        return std::unexpected{to_handshake_error(sent.error())};
    }

    // 2. The peer's first message must be its own version.
    auto first = peer.receive();
    if (!first) {
        return std::unexpected{to_handshake_error(first.error())};
    }

    if (first->header.command_name() != "version") {
        return std::unexpected{HandshakeErrorCode::unexpected_message};
    }

    auto peer_message = parse_payload(std::move(*first));
    if (!peer_message) {
        return std::unexpected{HandshakeErrorCode::protocol_error};
    }

    const auto* peer_version =
        std::get_if<VersionMessage>(&peer_message->payload);
    if (peer_version == nullptr) {
        return std::unexpected{HandshakeErrorCode::protocol_error};
    }

    HandshakeResult result{.version = *peer_version};

    // 3. Signal that we understand addrv2, which BIP155 places after the peer's
    //    version and before our verack. Older peers would disconnect on an
    //    unknown command, so only offer it to peers new enough to expect it.
    if (result.version.protocol_version >= addrv2_protocol_version) {
        if (
            const auto sent =
                peer.send("sendaddrv2", SendAddrV2Message{});
            !sent
        ) {
            return std::unexpected{to_handshake_error(sent.error())};
        }
    }

    // 4. Acknowledge the peer's version.
    if (const auto sent = peer.send("verack", VerackMessage{}); !sent) {
        return std::unexpected{to_handshake_error(sent.error())};
    }

    // 5. Wait for the peer's verack, ignoring any optional negotiation messages
    //    (sendheaders, sendcmpct, feefilter, wtxidrelay, ...) the peer may
    //    interleave before it. Its own sendaddrv2, if any, arrives here.
    while (true) {
        auto next = peer.receive();
        if (!next) {
            return std::unexpected{to_handshake_error(next.error())};
        }

        const auto command = next->header.command_name();

        if (command == "verack") {
            break;
        }

        if (command == "version") {
            // A second version is a protocol violation.
            return std::unexpected{HandshakeErrorCode::unexpected_message};
        }

        if (command == "sendaddrv2") {
            // The peer will accept addrv2 from us and may answer a getaddr
            // with one.
            result.supports_addrv2 = true;
        }

        // Anything else during the handshake is an optional negotiation
        // message; ignore it and keep waiting for the verack.
    }

    return result;
}
