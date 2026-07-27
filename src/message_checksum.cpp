#include "message_checksum.h"

#include <util/sha256.h>

#include <array>
#include <cstddef>
#include <span>

MessageChecksum calculate_message_checksum(std::span<const std::byte> payload)
{
    std::array<unsigned char, CSHA256::OUTPUT_SIZE> first_hash{};
    std::array<unsigned char, CSHA256::OUTPUT_SIZE> second_hash{};

    CSHA256{}
        .Write(reinterpret_cast<const unsigned char*>(payload.data()), payload.size())
        .Finalize(first_hash.data());

    CSHA256{}
        .Write(first_hash.data(), first_hash.size())
        .Finalize(second_hash.data());

    MessageChecksum checksum{};
    for (std::size_t i = 0; i < checksum.size(); ++i) {
        checksum[i] = static_cast<std::byte>(second_hash[i]);
    }
    return checksum;
}
