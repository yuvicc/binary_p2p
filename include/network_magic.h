#pragma once

#include <array>
#include <cstddef>

namespace NetworkMagic {

inline constexpr std::array<std::byte, 4> mainnet {
    std::byte{0xf9},
    std::byte{0xbe},
    std::byte{0xb4},
    std::byte{0xd9},
};

inline constexpr std::array<std::byte, 4> regtest {
    std::byte{0xfa},
    std::byte{0xbf},
    std::byte{0xb5},
    std::byte{0xda},
};

// Default (global) signet. A custom signet with its own -signetchallenge uses
// different magic derived from the challenge.
inline constexpr std::array<std::byte, 4> signet {
    std::byte{0x0a},
    std::byte{0x03},
    std::byte{0xcf},
    std::byte{0x40},
};

} // namespace NetworkMagic


