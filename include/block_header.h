#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// An 80-byte Bitcoin block header
struct BlockHeader {
    std::int32_t version{};
    std::array<std::byte, 32> previous_block_hash{};
    std::array<std::byte, 32> merkle_root{};
    std::uint32_t timestamp{};
    std::uint32_t bits{};
    std::uint32_t nonce{};

    friend bool operator==(
        const BlockHeader&,
        const BlockHeader&
    ) = default;
};
