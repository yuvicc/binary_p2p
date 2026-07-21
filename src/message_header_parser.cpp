#include "message_header_parser.h"

#include "util/reader.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

std::expected<MessageHeader, HeaderParseError>
parse_message_header(
    std::span<const std::byte> bytes,
    const std::array<std::byte, 4>& expected_magic
)
{
    if (bytes.size() < MessageHeader::encoded_size) {
        return std::unexpected{
            HeaderParseError{
                .code = HeaderParseErrorCode::insufficient_data,
                .position = bytes.size(),
            }
        };
    }

    ByteReader reader{
        bytes.first(MessageHeader::encoded_size)
    };

    const auto magic_result = reader.read_array<4>();
    if (!magic_result) {
        return std::unexpected{
            HeaderParseError{
                .code = HeaderParseErrorCode::insufficient_data,
                .position = reader.position(),
            }
        };
    }

    const auto command_result =
        reader.read_array<MessageHeader::command_size>();

    if (!command_result) {
        return std::unexpected{
            HeaderParseError{
                .code = HeaderParseErrorCode::insufficient_data,
                .position = reader.position(),
            }
        };
    }

    const auto payload_size_result = reader.read_u32_le();
    if (!payload_size_result) {
        return std::unexpected{
            HeaderParseError{
                .code = HeaderParseErrorCode::insufficient_data,
                .position = reader.position(),
            }
        };
    }

    const auto checksum_result = reader.read_array<4>();
    if (!checksum_result) {
        return std::unexpected{
            HeaderParseError{
                .code = HeaderParseErrorCode::insufficient_data,
                .position = reader.position(),
            }
        };
    }

    if (*magic_result != expected_magic) {
        return std::unexpected{
            HeaderParseError{
                .code = HeaderParseErrorCode::incorrect_magic,
                .position = 0,
            }
        };
    }

    std::array<char, MessageHeader::command_size> command{};

    for (std::size_t index = 0; index < command.size(); ++index) {
        command[index] = static_cast<char>(
            std::to_integer<unsigned char>(
                (*command_result)[index]
            )
        );
    }

    if (!MessageHeader::is_valid_command(command)) {
        return std::unexpected{
            HeaderParseError{
                .code = HeaderParseErrorCode::invalid_command,
                .position = 4,
            }
        };
    }

    if (*payload_size_result > MessageHeader::max_payload_size) {
        return std::unexpected{
            HeaderParseError{
                .code = HeaderParseErrorCode::payload_too_large,
                .position = 16,
            }
        };
    }

    return MessageHeader{
        *magic_result,
        command,
        *payload_size_result,
        *checksum_result
    };
}
