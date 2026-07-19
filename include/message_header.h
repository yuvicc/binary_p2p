#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>

class MessageHeader {

private:
    [[nodiscard]] static bool valid_character(char character);

public:
    static constexpr std::size_t command_size = 12;
    static constexpr std::uint32_t max_payload_size = 4'000'000;

    std::array<std::byte, 4> magic{};
    std::array<char, command_size> command{};
    std::uint32_t payload_size{};
    std::array<std::byte, 4> checksum{};

    explicit MessageHeader(std::string_view message_type, std::uint32_t payload_length = 0);
    [[nodiscard]] std::string_view command_name() const noexcept;

};
