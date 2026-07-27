#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

struct NetworkAddress {
    std::uint64_t services{};
    std::array<std::byte, 16> ip_address{};
    std::uint16_t port{};

    friend bool operator==(
        const NetworkAddress&,
        const NetworkAddress&
    ) = default;
};

struct VersionMessage {
    static constexpr std::size_t
        max_user_agent_length = 256;

    std::int32_t protocol_version{};
    std::uint64_t services{};
    std::int64_t timestamp{};

    NetworkAddress receiver_address{};
    NetworkAddress sender_address{};

    std::uint64_t nonce{};
    std::string user_agent;
    std::int32_t start_height{};

    // nullopt means that the optional relay field was absent.
    std::optional<bool> relay{};

    [[nodiscard]]
    bool relays_transactions() const noexcept
    {
        // An absent relay field has the same protocol meaning as true.
        return relay.value_or(true);
    }

    friend bool operator==(
        const VersionMessage&,
        const VersionMessage&
    ) = default;
};
