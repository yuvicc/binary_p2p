#pragma once

#include "inventory.h"
#include "parse_error.h"

#include <cstddef>
#include <expected>
#include <span>
#include <vector>

[[nodiscard]]
std::expected<std::vector<InventoryVector>, ParseError>
parse_inventory_payload(
    std::span<const std::byte> payload
);
