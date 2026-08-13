#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

// build up an output buffer
class ByteWriter {
public:
    ByteWriter() = default;

    explicit ByteWriter(std::size_t reserve)
    {
        m_bytes.reserve(reserve);
    }

    // number of bytes written so far
    [[nodiscard]] std::size_t size() const noexcept
    {
        return m_bytes.size();
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return m_bytes.empty();
    }

    // non-owning view of everything written so far
    [[nodiscard]] std::span<const std::byte> written_bytes() const noexcept
    {
        return m_bytes;
    }

    // hand the accumulated buffer to the caller
    [[nodiscard]] std::vector<std::byte> take() noexcept
    {
        return std::move(m_bytes);
    }

    void write_u8(std::uint8_t value);
    void write_u16_le(std::uint16_t value);
    void write_u16_be(std::uint16_t value);
    void write_u32_le(std::uint32_t value);
    void write_i32_le(std::int32_t value);
    void write_u64_le(std::uint64_t value);
    void write_i64_le(std::int64_t value);
    void write_bytes(std::span<const std::byte> bytes);

    // append exactly N bytes
    template<std::size_t N>
    void write_array(const std::array<std::byte, N>& bytes)
    {
        write_bytes(bytes);
    }

private:
    std::vector<std::byte> m_bytes;
};
