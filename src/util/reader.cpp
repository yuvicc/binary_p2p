#include "util/reader.h"
#include "util/parser.h"

#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

namespace {

template<std::unsigned_integral Integer>
[[nodiscard]]
Integer decode_little_endian(
    std::span<const std::byte> bytes
) noexcept
{
    Integer result{};

    for (std::size_t idx = 0; idx < bytes.size(); ++idx)
    {
        const auto byte_value = std::to_integer<std::uint8_t>(bytes[idx]);
        result |= static_cast<Integer>(byte_value) << static_cast<unsigned int>(idx * 8);
    }

    return result;
}

template<std::unsigned_integral Integer>
[[nodiscard]] Integer decode_big_endian(
    std::span<const std::byte> bytes
)
{
    Integer result{};

    for (const std::byte byte : bytes)
    {
        const auto byte_value = std::to_integer<std::uint8_t>(byte);

        result = static_cast<Integer>(
            (result << 8) | static_cast<Integer>(byte_value)
        );
    }

    return result;
}


} // namespace

std::expected<std::span<const std::byte>, ByteReaderError>
ByteReader::read_bytes(std::size_t count) noexcept
{
    if (count > remaining()) {
        return std::unexpected{insufficient_data_error(count)};
    }

    const auto result = m_bytes.subspan(m_position, count);

    m_position += count;

    return result;
}

std::expected<void, ByteReaderError>
ByteReader::skip(std::size_t count) noexcept
{
    auto bytes = read_bytes(count);

    if (!bytes) {
        return std::unexpected{bytes.error()};
    }

    return {};
}


std::expected<std::uint8_t, ByteReaderError> ByteReader::read_u8() noexcept
{
    auto bytes = read_bytes(sizeof(std::uint8_t));

    if (!bytes) {
        return std::unexpected{bytes.error()};
    }

    return std::to_integer<std::uint8_t>((*bytes)[0]);
}


std::expected<std::uint16_t, ByteReaderError> ByteReader::read_u16_le() noexcept
{
    auto bytes = read_bytes(sizeof(std::uint16_t));

    if (!bytes) {
        return std::unexpected{bytes.error()};
    }

    return decode_little_endian<std::uint16_t>(*bytes);
}

std::expected<std::uint16_t, ByteReaderError> ByteReader::read_u16_be() noexcept
{
    auto bytes = read_bytes(sizeof(std::uint16_t));

    if (!bytes) {
        return std::unexpected{bytes.error()};
    }

    return decode_big_endian<std::uint16_t>(*bytes);
}


std::expected<std::uint32_t, ByteReaderError> ByteReader::read_u32_le() noexcept
{
    auto bytes = read_bytes(sizeof(std::uint32_t));
    if (!bytes) {
        return std::unexpected{bytes.error()};
    }

    return decode_little_endian<std::uint32_t>(*bytes);
}

std::expected<std::int32_t, ByteReaderError> ByteReader::read_i32_le() noexcept
{
    auto bytes = read_bytes(sizeof(std::int32_t));
    if (!bytes) {
        return std::unexpected{bytes.error()};
    }

    return static_cast<std::int32_t>(decode_little_endian<std::uint32_t>(*bytes));
}

std::expected<std::uint64_t, ByteReaderError> ByteReader::read_u64_le() noexcept
{
    auto bytes = read_bytes(sizeof(std::uint64_t));
    if (!bytes) {
        return std::unexpected{bytes.error()};
    }

    return decode_little_endian<std::uint64_t>(*bytes);
}

std::expected<std::int64_t, ByteReaderError> ByteReader::read_i64_le() noexcept
{
    auto bytes = read_bytes(sizeof(std::int64_t));
    if (!bytes) {
        return std::unexpected{bytes.error()};
    }

    return static_cast<std::int64_t>(decode_little_endian<std::uint64_t>(*bytes));
}









