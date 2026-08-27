#pragma once

#include "address.h"
#include "parse_error.h"

#include <cstddef>
#include <expected>
#include <span>
#include <vector>

// Parses the body of an "addr" message.
[[nodiscard]]
std::expected<std::vector<AddressEntry>, ParseError>
parse_addr_payload(
    std::span<const std::byte> payload
);
