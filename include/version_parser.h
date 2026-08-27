#pragma once

#include "parse_error.h"
#include "version_message.h"

#include "util/reader.h"

#include <cstddef>
#include <expected>
#include <span>

// Parses a single 26-byte network address from the reader's current position.
// The "addr" message reuses this layout behind a timestamp.
[[nodiscard]]
std::expected<NetworkAddress, ParseError>
parse_network_address(ByteReader& reader);

[[nodiscard]]
std::expected<VersionMessage, ParseError>
parse_version_payload(
    std::span<const std::byte> payload
);
