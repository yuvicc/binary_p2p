#pragma once

#include "message_header.h"
#include "version_message.h"

#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

struct VerackMessage {
    friend constexpr bool operator==(
        const VerackMessage&,
        const VerackMessage&
    ) = default;
};

struct PingMessage {
    std::uint64_t nonce{};

    friend constexpr bool operator==(
        const PingMessage&,
        const PingMessage&
    ) = default;
};

struct PongMessage {
    std::uint64_t nonce{};

    friend constexpr bool operator==(
        const PongMessage&,
        const PongMessage&
    ) = default;
};

struct UnknownMessage {
    std::vector<std::byte> payload;

    friend bool operator==(
        const UnknownMessage&,
        const UnknownMessage&
    ) = default;
};

using MessagePayload = std::variant<
    VersionMessage,
    VerackMessage,
    PingMessage,
    PongMessage,
    UnknownMessage
>;

struct Message {
    MessageHeader header;
    MessagePayload payload;
};
