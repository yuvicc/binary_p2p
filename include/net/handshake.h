#pragma once

#include "net/peer.h"
#include "version_message.h"

#include <cstdint>
#include <expected>

enum class HandshakeErrorCode {
    connection_closed,  // the peer hung up mid-handshake
    io_error,           // a socket send/receive failed
    protocol_error,     // a message could not be framed or parsed
    unexpected_message, // the peer violated the handshake ordering
};

// What the handshake learned about the peer.
struct HandshakeResult {
    VersionMessage version;

    // True when the peer sent a sendaddrv2 before its verack. Only then may we
    // send it addrv2; otherwise addresses must go out as legacy addr.
    bool supports_addrv2{};
};

// The first protocol version that understands BIP155, and so the version at
// or above which sending a sendaddrv2 is meaningful.
inline constexpr std::int32_t addrv2_protocol_version = 70016;

// perform handshake for alreadty connected peer
[[nodiscard]]
std::expected<HandshakeResult, HandshakeErrorCode> perform_handshake(Peer& peer, const VersionMessage& local_version);
