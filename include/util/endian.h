// Copyright (c) 2014-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_COMPAT_ENDIAN_H
#define BITCOIN_COMPAT_ENDIAN_H

#include <bit>
#include <cstdint>

inline uint16_t htole16_internal(uint16_t x)
{
    if constexpr (std::endian::native == std::endian::big) return std::byteswap(x);
    return x;
}
inline uint32_t htole32_internal(uint32_t x)
{
    if constexpr (std::endian::native == std::endian::big) return std::byteswap(x);
    return x;
}
inline uint64_t htole64_internal(uint64_t x)
{
    if constexpr (std::endian::native == std::endian::big) return std::byteswap(x);
    return x;
}
inline uint16_t le16toh_internal(uint16_t x) { return htole16_internal(x); }
inline uint32_t le32toh_internal(uint32_t x) { return htole32_internal(x); }
inline uint64_t le64toh_internal(uint64_t x) { return htole64_internal(x); }

inline uint16_t htobe16_internal(uint16_t x)
{
    if constexpr (std::endian::native == std::endian::little) return std::byteswap(x);
    return x;
}
inline uint32_t htobe32_internal(uint32_t x)
{
    if constexpr (std::endian::native == std::endian::little) return std::byteswap(x);
    return x;
}
inline uint64_t htobe64_internal(uint64_t x)
{
    if constexpr (std::endian::native == std::endian::little) return std::byteswap(x);
    return x;
}
inline uint16_t be16toh_internal(uint16_t x) { return htobe16_internal(x); }
inline uint32_t be32toh_internal(uint32_t x) { return htobe32_internal(x); }
inline uint64_t be64toh_internal(uint64_t x) { return htobe64_internal(x); }

#endif // BITCOIN_COMPAT_ENDIAN_H
