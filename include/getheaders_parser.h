#pragma once

#include "message.h"
#include "parse_error.h"

#include <cstddef>
#include <expected>
#include <span>

// Parses the body of a "getheaders".
[[nodiscard]]
std::expected<GetHeadersMessage, ParseError>
parse_getheaders_payload(
    std::span<const std::byte> payload
);
