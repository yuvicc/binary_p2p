#pragma once

#include "message.h"
#include "parse_error.h"

#include <cstddef>
#include <expected>
#include <span>

// Parses the body of a "block" message=
[[nodiscard]]
std::expected<BlockMessage, ParseError>
parse_block_payload(
    std::span<const std::byte> payload
);
