#pragma once

#include "block_header.h"
#include "parse_error.h"
#include "util/reader.h"

#include <cstddef>
#include <expected>
#include <span>
#include <vector>

// Parses a single 80-byte block header from the reader's current position.
[[nodiscard]]
std::expected<BlockHeader, ParseError>
parse_block_header(ByteReader& reader);

// Parses the body of a "headers" message
[[nodiscard]]
std::expected<std::vector<BlockHeader>, ParseError>
parse_headers_payload(
    std::span<const std::byte> payload
);
