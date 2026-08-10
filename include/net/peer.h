#pragma once

#include "message.h"
#include "net/tcp_connection.h"
#include "message_stream_decoder.h"
#include "raw_message.h"

#include <array>
#include <cstddef>
#include <expected>
#include <string_view>

enum class PeerErrorCode {
    connection_closed, // the peer hung up
    io_error,          // a socket send/receive failed
    protocol_error,    // the bytes on the wire did not frame a valid message
};

// A connected Bitcoin peer
class Peer {
public:
    Peer(TcpConnection connection, std::array<std::byte, 4> magic);

    // Serialize a payload under the given command and send the complete frame.
    [[nodiscard]]
    std::expected<void, PeerErrorCode>
    send(std::string_view command, const MessagePayload& payload);

    // Block until a whole message has been received. Reads from the socket as
    // needed. Fails if the connection closes or the stream desynchronises.
    [[nodiscard]]
    std::expected<RawMessage, PeerErrorCode>
    receive();

private:
    TcpConnection m_connection;
    MessageStreamDecoder m_decoder;
    std::array<std::byte, 4> m_magic;
};
