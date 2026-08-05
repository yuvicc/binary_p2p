#pragma once

#include "block_header.h"
#include "parse_error.h"

#include <cstddef>
#include <expected>
#include <span>
#include <vector>

// Parses the body of a "headers" message
[[nodiscard]]
std::expected<std::vector<BlockHeader>, ParseError>
parse_headers_payload(
    std::span<const std::byte> payload
);
