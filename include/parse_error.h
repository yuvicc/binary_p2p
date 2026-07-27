#pragma once

enum class ParseError {
    insufficient_data,
    incorrect_magic,
    invalid_command,
    payload_too_large,
    checksum_mismatch,
    malformed_payload,
    trailing_bytes,

    non_canonical_compact_size,
    compact_size_too_large
};
