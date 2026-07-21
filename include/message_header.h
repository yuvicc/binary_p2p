#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>

class MessageHeader {
public:
    static constexpr std::size_t encoded_size = 24;
    static constexpr std::size_t command_size = 12;
    static constexpr std::uint32_t max_payload_size = 4'000'000;

    std::array<std::byte, 4> magic{};
    std::array<char, command_size> command{};
    std::uint32_t payload_size{};
    std::array<std::byte, 4> checksum{};

    // construction during an outgoinf message
    explicit MessageHeader(std::string_view message_type, std::uint32_t payload_length = 0);

    // construction during an incoming message
    MessageHeader(std::array<std::byte, 4> magic_val,
                  std::array<char, command_size> command_val,
                  std::uint32_t payload_size,
                  std::array<std::byte, 4> checksum_val);

    [[nodiscard]] std::string_view command_name() const noexcept;
    [[nodiscard]] bool is_valid_command(const std::array<char, command_size>& command) noexcept;

private:
    [[nodiscard]] static bool valid_character(char character) noexcept;
};
