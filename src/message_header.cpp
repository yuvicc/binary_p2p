#include "message_header.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>

bool MessageHeader::valid_character(char character) noexcept
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


MessageHeader::MessageHeader(std::array<std::byte, 4> magic_val,
    std::array<char, command_size> command_val,
    std::uint32_t payload_size,
    std::array<std::byte, 4> checksum_val)
    : magic{magic_val},
      command{command_val},
      payload_size{payload_size},
      checksum{checksum_val}
{
    if (!is_valid_command(command)) {
        throw std::invalid_argument{
            "invalid command filed"
        };
    }
    
    if (payload_size > max_payload_size) {
        throw std::invalid_argument{
            "payload exceeds maximum size"
        };
    }
}

bool MessageHeader::is_valid_command(
    const std::array<char, command_size>& command
) noexcept
{
    bool padding_started = false;

    for (const char character : command) {
        if (character == '\0') {
            padding_started = true;
            continue;
        }

        if (padding_started) {
            return false;
        }

        if (!valid_character(character)) {
            return false;
        }
    }

    return true;
}

