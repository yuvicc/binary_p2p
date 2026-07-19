#include "message_header.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string_view>

bool MessageHeader::valid_character(char character)
{
    const auto value = static_cast<unsigned char>(character);
    return value >= 0x20 && value <= 0x7e;
}

[[nodiscard]] std::string_view MessageHeader::command_name() const noexcept
{
    // command contains '\0' for unfilled indexes
    const auto end = std::ranges::find(command, '\0');
    return {command.begin(), end};
}

MessageHeader::MessageHeader(std::string_view message_type, std::uint32_t payload_length)
: payload_size{payload_length}
{
    if (message_type.empty()) {
        throw std::invalid_argument{"message type cannot be empty"};
    }
    if (message_type.size() > command.size()) {
        throw std::invalid_argument{"message type exceeds 12 bytes limit"};
    }
    if (payload_size > max_payload_size) {
        throw std::invalid_argument{"payload exceeds maximum size"};
    }
    if (!std::ranges::all_of(message_type, valid_character)) {
        throw std::invalid_argument{"message type contains non printable character"};
    }

    std::ranges::copy(message_type, command.begin());
}
