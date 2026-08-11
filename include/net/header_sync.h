#pragma once

#include "block_header.h"
#include "net/peer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <vector>

enum class SyncErrorCode {
    connection_closed, // the peer hung up
    io_error,          // a socket send/receive failed
    protocol_error,    // a message could not be framed or parsed
};

// Send a "getheaders" with the given locator and block until the peer's
// "headers" reply arrives, returning its block headers.
[[nodiscard]]
std::expected<std::vector<BlockHeader>, SyncErrorCode>
request_headers(
    Peer& peer,
    std::uint32_t protocol_version,
    const std::vector<std::array<std::byte, 32>>& locator,
    const std::array<std::byte, 32>& hash_stop = {}
);
