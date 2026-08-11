#include "block_hash.h"

#include "headers_serializer.h"
#include "util/writer.h"

#include <util/sha256.h>

#include <array>
#include <cstddef>

std::array<std::byte, 32> block_hash(const BlockHeader& header)
{
    ByteWriter writer;
    serialize_block_header(writer, header);
    const auto serialized = writer.take();

    std::array<unsigned char, CSHA256::OUTPUT_SIZE> first_hash{};
    std::array<unsigned char, CSHA256::OUTPUT_SIZE> second_hash{};

    CSHA256{}
        .Write(
            reinterpret_cast<const unsigned char*>(serialized.data()),
            serialized.size()
        )
        .Finalize(first_hash.data());

    CSHA256{}
        .Write(first_hash.data(), first_hash.size())
        .Finalize(second_hash.data());

    std::array<std::byte, 32> hash{};
    for (std::size_t i = 0; i < hash.size(); ++i) {
        hash[i] = static_cast<std::byte>(second_hash[i]);
    }
    return hash;
}
