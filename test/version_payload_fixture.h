#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <vector>

// Small byte-assembly helpers shared by the version/payload parser tests.
namespace test_support {

inline void put_u8(std::vector<std::byte>& out, std::uint8_t value)
{
    out.push_back(std::byte{value});
}

template<class T>
inline void put_le(std::vector<std::byte>& out, T value)
{
    using U = std::make_unsigned_t<T>;
    const auto bits = static_cast<U>(value);
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        put_u8(out, static_cast<std::uint8_t>(bits >> (8 * i)));
    }
}

inline void put_u16_be(std::vector<std::byte>& out, std::uint16_t value)
{
    put_u8(out, static_cast<std::uint8_t>(value >> 8));
    put_u8(out, static_cast<std::uint8_t>(value));
}

inline void put_str(std::vector<std::byte>& out, std::string_view text)
{
    for (const char c : text) {
        put_u8(out, static_cast<std::uint8_t>(c));
    }
}

// A network address is services(u64) + ip(16) + port(u16, big-endian).
inline void put_network_address(
    std::vector<std::byte>& out,
    std::uint64_t services,
    std::uint16_t port
)
{
    put_le<std::uint64_t>(out, services);
    for (int i = 0; i < 16; ++i) {
        put_u8(out, 0x00);
    }
    put_u16_be(out, port);
}

// Builds a canonical version payload. relay: -1 absent, 0 false, 1 true.
inline std::vector<std::byte> build_version_payload(int relay = 1)
{
    std::vector<std::byte> out;
    put_le<std::int32_t>(out, 70015);
    put_le<std::uint64_t>(out, 1);
    put_le<std::int64_t>(out, 1231006505);
    put_network_address(out, 1, 8333);
    put_network_address(out, 0, 0);
    put_le<std::uint64_t>(out, 0x1122334455667788ULL);

    constexpr std::string_view user_agent = "/Satoshi:1.0/";
    put_u8(out, static_cast<std::uint8_t>(user_agent.size()));
    put_str(out, user_agent);

    put_le<std::int32_t>(out, 700000);
    if (relay >= 0) {
        put_u8(out, static_cast<std::uint8_t>(relay));
    }
    return out;
}

} // namespace test_support
