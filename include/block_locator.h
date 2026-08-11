#pragma once

#include <array>
#include <cstddef>
#include <vector>

// Build a block locator from a chain of block hashes ordered genesis-first
// (index 0 is genesis, back() is the tip). The locator lists recent hashes
// densely and older ones with exponentially growing gaps, always ending at
// genesis, so a peer can quickly find the fork point with our chain.
[[nodiscard]]
std::vector<std::array<std::byte, 32>>
build_block_locator(const std::vector<std::array<std::byte, 32>>& chain_hashes);
