#pragma once

enum class ParseError {
    insufficient_data,
    incorrect_magic,
    invalid_command,
    payload_too_large,
    checksum_mismatch,
    malformed_payload,
    trailing_bytes
};
