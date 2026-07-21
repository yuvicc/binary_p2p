#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

enum class ByteReaderErrorCode {
    insufficient_data,
};

struct ByteReaderError {
    ByteReaderErrorCode code;
    std::size_t position;
    std::size_t requested;
    std::size_t available;

    friend constexpr bool operator==(
        const ByteReaderError&,
        const ByteReaderError&
    ) = default;
};

// read and parse the input buffer
class ByteReader {
public:
    explicit constexpr ByteReader(std::span<const std::byte> bytes) noexcept
    : m_bytes{bytes} 
    { }

    // return size of buffer
    [[nodiscard]] constexpr std::size_t size() const noexcept
    {
        return m_bytes.size();
    }

    [[nodiscard]] constexpr std::size_t position() const noexcept
    {
        return m_position;
    }

    [[nodiscard]] constexpr std::size_t remaining() const noexcept
    {
        return size() - position();
    }

    [[nodiscard]] constexpr bool empty() const noexcept
    {
        return remaining() == 0;
    }

    //  returns the non-owning view of buffer(bytes) from the current position to the end.
    [[nodiscard]] constexpr std::span<const std::byte>
    unread_bytes() const noexcept
    {
        return m_bytes.subspan(m_position);
    }

    // read one unsigned byte
    [[nodiscard]] std::expected<std::uint8_t, ByteReaderError>
    read_u8() noexcept;

    // read two-byte unsigned integer in LE order
    [[nodiscard]] std::expected<std::uint16_t, ByteReaderError>
    read_u16_le() noexcept;

    // read two-byte unsigned integer in BE order
    [[nodiscard]] std::expected<std::uint16_t, ByteReaderError>
    read_u16_be() noexcept;

    // read a four-byte unsigned integer in LW order
    [[nodiscard]] std::expected<std::uint32_t, ByteReaderError>
    read_u32_le() noexcept;

    // read a four byte signed integer in LE order
    [[nodiscard]] std::expected<std::int32_t, ByteReaderError>
    read_i32_le() noexcept;

    // read an eight byte unsigned integer in LW order
    [[nodiscard]] std::expected<std::uint64_t, ByteReaderError>
    read_u64_le() noexcept;

    [[nodiscard]] std::expected<std::int64_t, ByteReaderError>
    read_i64_le() noexcept;

    // returns the non-owning view of next `count` bytes and advances the cursor as well.
    // an error is returned if fewer `count` bytes remain.
    [[nodiscard]] std::expected<std::span<const std::byte>, ByteReaderError>
    read_bytes(std::size_t count) noexcept;

    // copies exactly N bytes into an owning std::array
    template<std::size_t N>
    [[nodiscard]] std::expected<std::array<std::byte, N>, ByteReaderError>
    read_array() noexcept
    {
        auto bytes = read_bytes(N);

        if (!bytes) {
            return std::unexpected{bytes.error()};
        }

        std::array<std::byte, N> bytes_array{};
        std::ranges::copy(*bytes, bytes_array.begin());

        return bytes_array;
    }

    // advances the cursor without returning anything
    [[nodiscard]] std::expected<void, ByteReaderError>
    skip(std::size_t N) noexcept;

private:

    [[nodiscard]]
    constexpr ByteReaderError
    insufficient_data_error(std::size_t requested) const noexcept
    {
        return ByteReaderError{
            .code = ByteReaderErrorCode::insufficient_data,
            .position = m_position,
            .requested = requested,
            .available = remaining(),
        };
    }

    std::span<const std::byte> m_bytes;
    std::size_t m_position{};
};
