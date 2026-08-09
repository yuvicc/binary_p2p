#pragma once

#include "message.h"

#include <array>
#include <cstddef>
#include <string_view>
#include <vector>

[[nodiscard]]
std::vector<std::byte> serialize_payload(const MessagePayload& payload);

[[nodiscard]]
std::vector<std::byte> serialize_message(
    std::array<std::byte, 4> magic,
    std::string_view command,
    const MessagePayload& payload
);

[[nodiscard]]
std::vector<std::byte> serialize_message(const Message& message);
