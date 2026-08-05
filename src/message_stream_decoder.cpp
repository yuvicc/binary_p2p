#include "message_stream_decoder.h"

#include "raw_message_parser.h"

#include <cstddef>
#include <expected>
#include <optional>
#include <span>
#include <utility>
#include <vector>

MessageStreamDecoder::MessageStreamDecoder(
    std::array<std::byte, 4> expected_magic
)
: m_magic{expected_magic}
{ }

void MessageStreamDecoder::feed(std::span<const std::byte> bytes)
{
    // Reclaim the already-consumed prefix before growing the buffer so a
    // long-lived connection does not accumulate dead bytes without bound.
    discard_consumed();
    m_buffer.insert(m_buffer.end(), bytes.begin(), bytes.end());
}

std::expected<std::optional<RawMessage>, ParseError>
MessageStreamDecoder::next()
{
    const std::span<const std::byte> unread =
        std::span<const std::byte>{m_buffer}.subspan(m_read_position);

    auto result = parse_raw_message(unread, m_magic);

    if (result) {
        m_read_position += result->bytes_consumed;
        return std::optional<RawMessage>{std::move(result->message)};
    }

    // Incomplete framing is not an error
    if (result.error() == ParseError::insufficient_data) {
        return std::optional<RawMessage>{std::nullopt};
    }

    return std::unexpected{result.error()};
}

std::size_t MessageStreamDecoder::buffered() const noexcept
{
    return m_buffer.size() - m_read_position;
}

void MessageStreamDecoder::discard_consumed()
{
    if (m_read_position == 0) {
        return;
    }

    m_buffer.erase(
        m_buffer.begin(),
        m_buffer.begin() +
            static_cast<std::vector<std::byte>::difference_type>(
                m_read_position
            )
    );
    m_read_position = 0;
}
