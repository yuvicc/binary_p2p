#include "compact_size.h"

#include <cstddef>
#include <cstdint>
#include <expected>

std::expected<std::uint64_t, ParseError>
read_compact_size(
    ByteReader& reader,
    std::uint64_t max) noexcept
{
    const auto unread = reader.unread_bytes();

    if (unread.empty()) {
        return std::unexpected{
            ParseError::insufficient_data
        };
    }

    const auto prefix = std::to_integer<std::uint8_t>(unread[0]);

    std::uint64_t value{};
    std::size_t encoded_size{};

    if (prefix < 0xfd) {
        value = prefix;
        encoded_size = 1;
    }
    else if (prefix == 0xfd) {
        
        if (unread.size() < 3) {
            return std::unexpected{ParseError::insufficient_data};
        }

        ByteReader value_reader{unread.subspan(1, 2)};

        const auto decoded = value_reader.read_u16_le();

        if (!decoded) {
            return std::unexpected{ParseError::insufficient_data};
        }
        
        value = *decoded;
        encoded_size = 3;

        if (value < 0xfd) {
            return std::unexpected{ParseError::non_canonical_compact_size};
        }

    }
    else if (prefix == 0xfe) {
        if (unread.size() < 5) {
            return std::unexpected{ParseError::insufficient_data};
        }

        ByteReader value_reader{unread.subspan(1, 4)};

        const auto decoded = value_reader.read_u32_le();
    
        if (!decoded) {
            return std::unexpected{ParseError::insufficient_data};
        }

        value = *decoded;
        encoded_size = 5;

        if (value < 0x1'0000ULL) {
            return std::unexpected{ParseError::non_canonical_compact_size};
        }
    }
    else {
        if (unread.size() < 9) {
            return std::unexpected{ParseError::insufficient_data};
        }

        ByteReader value_reader{unread.subspan(1, 8)};
        const auto decoded = value_reader.read_u64_le();

        if (!decoded) {
            return std::unexpected{ParseError::insufficient_data};
        }

        value = *decoded;
        encoded_size = 9;

        if (value < 0x1'0000'0000ULL) {
            return std::unexpected{ParseError::non_canonical_compact_size};
        }
    }

    if (value > max) {
        return std::unexpected{ParseError::compact_size_too_large};
    }

    const auto skip_result = reader.skip(encoded_size);
    if (!skip_result) {
        return std::unexpected{ParseError::insufficient_data};
    }

    return value;
}

void write_compact_size(ByteWriter& writer, std::uint64_t value)
{
    if (value < 0xfd) {
        writer.write_u8(static_cast<std::uint8_t>(value));
    }
    else if (value <= 0xffffULL) {
        writer.write_u8(0xfd);
        writer.write_u16_le(static_cast<std::uint16_t>(value));
    }
    else if (value <= 0xffff'ffffULL) {
        writer.write_u8(0xfe);
        writer.write_u32_le(static_cast<std::uint32_t>(value));
    }
    else {
        writer.write_u8(0xff);
        writer.write_u64_le(value);
    }
}

















