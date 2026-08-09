#include "util/writer.h"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>

namespace {

template<std::unsigned_integral Integer>
void encode_little_endian(std::vector<std::byte>& out, Integer value)
{
    for (std::size_t idx = 0; idx < sizeof(Integer); ++idx)
    {
        const auto shift = static_cast<unsigned int>(idx * 8);
        out.push_back(static_cast<std::byte>((value >> shift) & 0xFFU));
    }
}

template<std::unsigned_integral Integer>
void encode_big_endian(std::vector<std::byte>& out, Integer value)
{
    for (std::size_t idx = 0; idx < sizeof(Integer); ++idx)
    {
        const auto shift =
            static_cast<unsigned int>((sizeof(Integer) - 1 - idx) * 8);
        out.push_back(static_cast<std::byte>((value >> shift) & 0xFFU));
    }
}

} // namespace

void ByteWriter::write_u8(std::uint8_t value)
{
    m_bytes.push_back(static_cast<std::byte>(value));
}

void ByteWriter::write_u16_le(std::uint16_t value)
{
    encode_little_endian<std::uint16_t>(m_bytes, value);
}

void ByteWriter::write_u16_be(std::uint16_t value)
{
    encode_big_endian<std::uint16_t>(m_bytes, value);
}

void ByteWriter::write_u32_le(std::uint32_t value)
{
    encode_little_endian<std::uint32_t>(m_bytes, value);
}

void ByteWriter::write_i32_le(std::int32_t value)
{
    encode_little_endian<std::uint32_t>(m_bytes, static_cast<std::uint32_t>(value));
}

void ByteWriter::write_u64_le(std::uint64_t value)
{
    encode_little_endian<std::uint64_t>(m_bytes, value);
}

void ByteWriter::write_i64_le(std::int64_t value)
{
    encode_little_endian<std::uint64_t>(m_bytes, static_cast<std::uint64_t>(value));
}

void ByteWriter::write_bytes(std::span<const std::byte> bytes)
{
    m_bytes.insert(m_bytes.end(), bytes.begin(), bytes.end());
}
