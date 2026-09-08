#pragma once

#include "addressv2.h"
#include "net/header_sync.h"
#include "net/peer.h"

#include <expected>
#include <vector>

// Send a "getaddr" and block until the peer's reply arrives, returning the
// addresses it advertised.
//
// A peer answers a getaddr with either an "addr" or, once sendaddrv2 has been
// negotiated, an "addrv2". Both are accepted here and legacy entries are
// widened to the addrv2 form so that callers deal with a single
// representation; see to_addressv2().
//
// There is no getaddrv2: the same getaddr asks for either format, and which
// one comes back was settled during the handshake.
//
// A peer is under no obligation to answer a getaddr, so this blocks until it
// does or the connection drops.
[[nodiscard]]
std::expected<std::vector<AddressV2Entry>, SyncErrorCode>
request_addresses(Peer& peer);
