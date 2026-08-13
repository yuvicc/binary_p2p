#pragma once

#include "parse_error.h"
#include "raw_message.h"

#include <array>
#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <vector>

// Reassembles a byte stream into whole, checksum-verified messages. 
// Bytes are buffered until a complete message can be produced.
class MessageStreamDecoder {
public:
    explicit MessageStreamDecoder(
        std::array<std::byte, 4> expected_magic
    );

    // Append newly received bytes to the internal buffer.
    void feed(std::span<const std::byte> bytes);

    [[nodiscard]]
    std::expected<std::optional<RawMessage>, ParseError> next();

    // Number of buffered bytes not yet consumed.
    [[nodiscard]] std::size_t buffered() const noexcept;

private:
    void discard_consumed();

    std::array<std::byte, 4> m_magic;
    std::vector<std::byte> m_buffer;
    std::size_t m_read_position{};
};
