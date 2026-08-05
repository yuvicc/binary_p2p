#pragma once

#include "parse_error.h"
#include "raw_message.h"

#include <array>
#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <vector>

// Reassembles a byte stream (for example a TCP connection) into whole,
// checksum-verified messages. Received bytes may arrive in arbitrary fragments:
// a partial header, a full header plus part of a payload, or several messages
// at once. Bytes are buffered until a complete message can be produced.
//
// Not thread-safe; use one decoder per connection.
class MessageStreamDecoder {
public:
    explicit MessageStreamDecoder(
        std::array<std::byte, 4> expected_magic
    );

    // Append newly received bytes to the internal buffer.
    void feed(std::span<const std::byte> bytes);

    // Pull the next complete message from the buffered bytes:
    //   - value holding a RawMessage: one whole message was available; the
    //     buffer has been advanced past it.
    //   - value holding std::nullopt: not enough bytes yet; feed() more and
    //     call again.
    //   - error: a protocol violation (bad magic, checksum mismatch, oversized
    //     payload, ...). The stream is desynchronised; the caller should stop
    //     decoding and drop the connection.
    [[nodiscard]]
    std::expected<std::optional<RawMessage>, ParseError> next();

    // Number of buffered bytes not yet consumed by a successful next().
    [[nodiscard]] std::size_t buffered() const noexcept;

private:
    void discard_consumed();

    std::array<std::byte, 4> m_magic;
    std::vector<std::byte> m_buffer;
    std::size_t m_read_position{};
};
