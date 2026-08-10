#include "getheaders_parser.h"

#include "compact_size.h"
#include "util/reader.h"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace {

// Bitcoin caps a block locator at MAX_LOCATOR_SZ hashes.
constexpr std::uint64_t max_locator_hashes = 101;

} // namespace

std::expected<GetHeadersMessage, ParseError>
parse_getheaders_payload(
    std::span<const std::byte> payload
)
{
    ByteReader reader{payload};

    const auto protocol_version = reader.read_u32_le();

    if (!protocol_version) {
        return std::unexpected{ParseError::malformed_payload};
    }

    const auto count = read_compact_size(reader, max_locator_hashes);

    if (!count) {
        // The RawMessage already holds the complete declared payload, so a
        // truncated CompactSize means the payload is malformed rather than
        // waiting for more network data.
        if (count.error() == ParseError::insufficient_data) {
            return std::unexpected{ParseError::malformed_payload};
        }

        return std::unexpected{count.error()};
    }

    GetHeadersMessage message;
    message.protocol_version = *protocol_version;
    message.block_locator_hashes.reserve(static_cast<std::size_t>(*count));

    for (std::uint64_t index = 0; index < *count; ++index) {
        const auto hash = reader.read_array<32>();

        if (!hash) {
            return std::unexpected{ParseError::malformed_payload};
        }

        message.block_locator_hashes.push_back(*hash);
    }

    const auto hash_stop = reader.read_array<32>();

    if (!hash_stop) {
        return std::unexpected{ParseError::malformed_payload};
    }

    message.hash_stop = *hash_stop;

    if (!reader.empty()) {
        return std::unexpected{ParseError::trailing_bytes};
    }

    return message;
}
