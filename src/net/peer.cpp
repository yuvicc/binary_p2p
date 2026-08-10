#include "net/peer.h"

#include "message_serializer.h"

#include <array>
#include <optional>
#include <span>
#include <utility>

Peer::Peer(TcpConnection connection, std::array<std::byte, 4> magic)
: m_connection{std::move(connection)}
, m_decoder{magic}
, m_magic{magic}
{ }

std::expected<void, PeerErrorCode>
Peer::send(std::string_view command, const MessagePayload& payload)
{
    const auto frame = serialize_message(m_magic, command, payload);

    if (!m_connection.send_all(frame)) {
        return std::unexpected{PeerErrorCode::io_error};
    }

    return {};
}

std::expected<RawMessage, PeerErrorCode>
Peer::receive()
{
    std::array<std::byte, 8192> buffer{};

    while (true) {
        auto next = m_decoder.next();

        if (!next) {
            // A framing error means the stream is desynchronised.
            return std::unexpected{PeerErrorCode::protocol_error};
        }

        if (next->has_value()) {
            return std::move(**next);
        }

        // Not enough buffered bytes yet; pull more from the socket.
        const auto received = m_connection.receive(buffer);

        if (!received) {
            return std::unexpected{PeerErrorCode::io_error};
        }

        if (*received == 0) {
            return std::unexpected{PeerErrorCode::connection_closed};
        }

        m_decoder.feed(std::span{buffer}.first(*received));
    }
}
