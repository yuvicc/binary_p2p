#pragma once

#include <array>
#include <cstddef>
#include <span>

using MessageChecksum = std::array<std::byte, 4>;

// this only returns the checksum which is stored in the message header
[[nodiscard]] MessageChecksum calculate_message_checksum(
    std::span<const std::byte> payload
);


