#pragma once

#include "net/peer.h"
#include "version_message.h"

#include <expected>

enum class HandshakeErrorCode {
    connection_closed,  // the peer hung up mid-handshake
    io_error,           // a socket send/receive failed
    protocol_error,     // a message could not be framed or parsed
    unexpected_message, // the peer violated the handshake ordering
};

// perform handshake for alreadty connected peer
[[nodiscard]]
std::expected<VersionMessage, HandshakeErrorCode> perform_handshake(Peer& peer, const VersionMessage& local_version);
