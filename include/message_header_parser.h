#pragma once

#include "message_header.h"

#include <array>
#include <cstddef>
#include <expected>
#include <span>

enum class HeaderParseErrorCode {
    insufficient_data,
    incorrect_magic,
    invalid_command,
    payload_too_large,
};

struct HeaderParseError {
    HeaderParseErrorCode code{};
    std::size_t position{};

    friend constexpr bool operator==(
        const HeaderParseError&,
        const HeaderParseError&
    ) = default;


};


[[nodiscard]]
std::expected<MessageHeader, HeaderParseError> parse_message_header(
    std::span<const std::byte> bytes,
    const std::array<std::byte, 4>& expected_magic
);


















































