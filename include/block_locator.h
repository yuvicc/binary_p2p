#pragma once

#include <array>
#include <cstddef>
#include <vector>

// Build a block locator from a chain of block hashes ordered genesis-first
[[nodiscard]]
std::vector<std::array<std::byte, 32>>
build_block_locator(const std::vector<std::array<std::byte, 32>>& chain_hashes);
