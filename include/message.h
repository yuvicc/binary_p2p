#pragma once

#include "block_header.h"
#include "inventory.h"
#include "message_header.h"
#include "version_message.h"

#include <array>
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

struct InvMessage {
    std::vector<InventoryVector> inventory;

    friend bool operator==(
        const InvMessage&,
        const InvMessage&
    ) = default;
};

struct GetdataMessage {
    std::vector<InventoryVector> inventory;

    friend bool operator==(
        const GetdataMessage&,
        const GetdataMessage&
    ) = default;
};

struct HeadersMessage {
    std::vector<BlockHeader> headers;

    friend bool operator==(
        const HeadersMessage&,
        const HeadersMessage&
    ) = default;
};

struct GetHeadersMessage {
    std::uint32_t protocol_version{};
    std::vector<std::array<std::byte, 32>> block_locator_hashes;
    std::array<std::byte, 32> hash_stop{};

    friend bool operator==(
        const GetHeadersMessage&,
        const GetHeadersMessage&
    ) = default;
};

struct BlockMessage {
    BlockHeader header;
    std::uint64_t transaction_count{};
    std::vector<std::byte> transactions;

    friend bool operator==(
        const BlockMessage&,
        const BlockMessage&
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
    InvMessage,
    GetdataMessage,
    HeadersMessage,
    GetHeadersMessage,
    BlockMessage,
    UnknownMessage
>;

struct Message {
    MessageHeader header;
    MessagePayload payload;
};
