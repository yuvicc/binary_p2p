#pragma once

#include "parse_error.h"
#include "version_message.h"

#include <cstddef>
#include <expected>
#include <span>

[[nodiscard]]
std::expected<VersionMessage, ParseError>
parse_version_payload(
    std::span<const std::byte> payload
);
