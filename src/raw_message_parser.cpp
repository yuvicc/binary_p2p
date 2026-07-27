#include "raw_message_parser.h"

#include "message_checksum.h"
#include "message_header_parser.h"

#include <cstddef>
#include <expected>
#include <span>
#include <utility>
#include <vector>

namespace {

[[nodiscard]] ParseError to_parse_error(HeaderParseErrorCode code)
{
    switch (code) {
        case HeaderParseErrorCode::insufficient_data:
            return ParseError::insufficient_data;
        case HeaderParseErrorCode::incorrect_magic:
            return ParseError::incorrect_magic;
        case HeaderParseErrorCode::invalid_command:
            return ParseError::invalid_command;
        case HeaderParseErrorCode::payload_too_large:
            return ParseError::payload_too_large;
    }
    return ParseError::malformed_payload;
}

} // namespace

std::expected<ParsedRawMessage, ParseError>
parse_raw_message(
    std::span<const std::byte> bytes,
    const std::array<std::byte, 4>& expected_magic
)
{
    const auto header_result =
        parse_message_header(bytes, expected_magic);

    if (!header_result) {
        return std::unexpected{to_parse_error(header_result.error().code)};
    }

    const auto& header = *header_result;

    // parse_message_header() already guarantees that at least
    // MessageHeader::encoded_size bytes are present.
    const std::size_t available_payload_size =
        bytes.size() - MessageHeader::encoded_size;

    const std::size_t required_payload_size =
        static_cast<std::size_t>(header.payload_size);

    if (required_payload_size > available_payload_size) {
        return std::unexpected{
            ParseError::insufficient_data
        };
    }

    const auto payload_view = bytes.subspan(
        MessageHeader::encoded_size,
        required_payload_size
    );

    // Validate before copying untrusted payload data into an owned vector.
    const auto calculated_checksum =
        calculate_message_checksum(payload_view);

    if (calculated_checksum != header.checksum) {
        return std::unexpected{
            ParseError::checksum_mismatch
        };
    }

    std::vector<std::byte> owned_payload{
        payload_view.begin(),
        payload_view.end()
    };

    const std::size_t bytes_consumed =
        MessageHeader::encoded_size + required_payload_size;

    return ParsedRawMessage{
        .message = RawMessage{
            .header = std::move(*header_result),
            .payload = std::move(owned_payload),
        },
        .bytes_consumed = bytes_consumed,
    };
}
