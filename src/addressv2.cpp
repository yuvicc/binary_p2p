#include "addressv2.h"

#include <algorithm>
#include <array>

namespace {

// The 80-bit zero run plus 0xffff that marks an IPv4-mapped IPv6 address.
constexpr std::array<std::byte, 12> ipv4_mapped_prefix{
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0x00}, std::byte{0x00},
    std::byte{0x00}, std::byte{0x00}, std::byte{0xff}, std::byte{0xff},
};

[[nodiscard]]
bool is_ipv4_mapped(const std::array<std::byte, 16>& ip) noexcept
{
    return std::equal(
        ipv4_mapped_prefix.begin(),
        ipv4_mapped_prefix.end(),
        ip.begin()
    );
}

} // namespace

std::optional<std::size_t>
expected_address_size(std::uint8_t network_id) noexcept
{
    switch (network_id) {
    case AddressV2Network::ipv4:  return 4;
    case AddressV2Network::ipv6:  return 16;
    case AddressV2Network::torv2: return 10;
    case AddressV2Network::torv3: return 32;
    case AddressV2Network::i2p:   return 32;
    case AddressV2Network::cjdns: return 16;
    default:                      return std::nullopt;
    }
}

bool is_usable_address(const AddressV2Entry& entry) noexcept
{
    const auto size = expected_address_size(entry.network_id);

    return size.has_value() && *size == entry.address.size();
}

AddressV2Entry to_addressv2(const AddressEntry& entry)
{
    const auto& ip = entry.address.ip_address;

    AddressV2Entry converted{
        .timestamp = entry.timestamp,
        .services = entry.address.services,
        .network_id = AddressV2Network::ipv6,
        .address = {ip.begin(), ip.end()},
        .port = entry.address.port,
    };

    if (is_ipv4_mapped(ip)) {
        converted.network_id = AddressV2Network::ipv4;
        converted.address.assign(ip.begin() + 12, ip.end());
    }

    return converted;
}
