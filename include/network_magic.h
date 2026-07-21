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

} // namespace NetworkMagic


