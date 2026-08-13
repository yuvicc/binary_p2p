#pragma once

#include "block_header.h"

#include <array>
#include <cstddef>

// block hash, the double-SHA256 of the serialized 80-byte headerd
[[nodiscard]]
std::array<std::byte, 32> block_hash(const BlockHeader& header);
