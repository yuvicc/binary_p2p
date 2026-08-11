#pragma once

#include "inventory.h"
#include "message.h"
#include "net/header_sync.h" // SyncErrorCode
#include "net/peer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>

// Request a single block by hash (getdata) and block until the peer's "block"
// reply arrives
[[nodiscard]]
std::expected<BlockMessage, SyncErrorCode>
request_block(
    Peer& peer,
    const std::array<std::byte, 32>& hash,
    std::uint32_t inventory_type = InventoryType::block
);
