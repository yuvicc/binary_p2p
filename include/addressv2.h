#pragma once

#include "address.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

// BIP155 network identifiers.
namespace AddressV2Network {

inline constexpr std::uint8_t ipv4 = 1;
inline constexpr std::uint8_t ipv6 = 2;
inline constexpr std::uint8_t torv2 = 3; // deprecated, no longer relayed
inline constexpr std::uint8_t torv3 = 4;
inline constexpr std::uint8_t i2p = 5;
inline constexpr std::uint8_t cjdns = 6;

} // namespace AddressV2Network

// One entry of an "addrv2" message. Unlike the fixed 16-byte field of a
// legacy addr, the address is a variable-length blob whose meaning depends on
// network_id, which lets it carry Tor v3, I2P and CJDNS addresses.
struct AddressV2Entry {
    // BIP155 caps a single address blob at 512 bytes.
    static constexpr std::size_t max_address_size = 512;

    std::uint32_t timestamp{};
    std::uint64_t services{};
    std::uint8_t network_id{};
    std::vector<std::byte> address;
    std::uint16_t port{};

    friend bool operator==(
        const AddressV2Entry&,
        const AddressV2Entry&
    ) = default;
};

// The address length BIP155 fixes for a known network, or nullopt for a
// network id this build does not recognise.
[[nodiscard]]
std::optional<std::size_t>
expected_address_size(std::uint8_t network_id) noexcept;

// True when the entry names a known network and carries the address length
// that network requires. A peer may legitimately relay entries that fail this
// check -- a newer network id, or a blob of the wrong size -- and BIP155 asks
// that those be ignored rather than treated as a malformed message, so the
// parser keeps them and leaves the filtering to the caller.
[[nodiscard]]
bool is_usable_address(const AddressV2Entry& entry) noexcept;

// Re-encodes a legacy addr entry in the addrv2 form. The 16-byte field becomes
// either a 4-byte ipv4 address (when it holds the IPv4-mapped IPv6 prefix) or a
// 16-byte ipv6 address, which is how the two formats line up on the wire.
[[nodiscard]]
AddressV2Entry to_addressv2(const AddressEntry& entry);
