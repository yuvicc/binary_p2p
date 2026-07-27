#pragma once

#include "parse_error.h"
#include "raw_message.h"

#include <array>
#include <cstddef>
#include <expected>
#include <span>

[[nodiscard]]
std::expected<ParsedRawMessage, ParseError>
parse_raw_message(
    std::span<const std::byte> bytes,
    const std::array<std::byte, 4>& expected_magic
);
