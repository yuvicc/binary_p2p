#pragma once

#include "message_header.h"

#include <cstddef>
#include <expected>
#include <span>

enum ParseError {
    insufficient_data,
    incorrect_mgic,
    invalid_command,
    payload_too_large,
    checksum_mismatch,
    malformed_payload,
    trailing_bytes
};
using ParseType = std::expected<MessageHeader, ParseError>;
using ParsedType = std::expected<std::span<const std::byte>, ParseError>;
