#pragma once

#include "addressv2.h"
#include "parse_error.h"

#include <cstddef>
#include <expected>
#include <span>
#include <vector>

// Parses the body of an "addrv2" message.
[[nodiscard]]
std::expected<std::vector<AddressV2Entry>, ParseError>
parse_addrv2_payload(
    std::span<const std::byte> payload
);
