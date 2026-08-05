#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace InventoryType {

inline constexpr std::uint32_t error = 0;
inline constexpr std::uint32_t tx = 1;
inline constexpr std::uint32_t block = 2;
inline constexpr std::uint32_t filtered_block = 3;
inline constexpr std::uint32_t compact_block = 4;
inline constexpr std::uint32_t witness_tx = 0x4000'0001;
inline constexpr std::uint32_t witness_block = 0x4000'0002;
inline constexpr std::uint32_t filtered_witness_block = 0x4000'0003;

} // namespace InventoryType

struct InventoryVector {
    std::uint32_t type{};
    std::array<std::byte, 32> hash{};

    friend bool operator==(
        const InventoryVector&,
        const InventoryVector&
    ) = default;
};
