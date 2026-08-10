#include "headers_parser.h"

#include "compact_size.h"
#include "util/reader.h"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <utility>
#include <vector>

namespace {

// Bitcoin caps a headers message at MAX_HEADERS_RESULTS entries.
constexpr std::uint64_t max_headers_count = 2000;

} // namespace

std::expected<BlockHeader, ParseError>
parse_block_header(ByteReader& reader)
{
    const auto version = reader.read_i32_le();

    if (!version) {
        return std::unexpected{ParseError::malformed_payload};
    }

    const auto previous_block_hash = reader.read_array<32>();

    if (!previous_block_hash) {
        return std::unexpected{ParseError::malformed_payload};
    }

    const auto merkle_root = reader.read_array<32>();

    if (!merkle_root) {
        return std::unexpected{ParseError::malformed_payload};
    }

    const auto timestamp = reader.read_u32_le();

    if (!timestamp) {
        return std::unexpected{ParseError::malformed_payload};
    }

    const auto bits = reader.read_u32_le();

    if (!bits) {
        return std::unexpected{ParseError::malformed_payload};
    }

    const auto nonce = reader.read_u32_le();

    if (!nonce) {
        return std::unexpected{ParseError::malformed_payload};
    }

    return BlockHeader{
        .version = *version,
        .previous_block_hash = *previous_block_hash,
        .merkle_root = *merkle_root,
        .timestamp = *timestamp,
        .bits = *bits,
        .nonce = *nonce,
    };
}

std::expected<std::vector<BlockHeader>, ParseError>
parse_headers_payload(
    std::span<const std::byte> payload
)
{
    ByteReader reader{payload};

    const auto count = read_compact_size(reader, max_headers_count);

    if (!count) {
        // The RawMessage already holds the complete declared payload, so a
        // truncated CompactSize means the payload is malformed rather than
        // waiting for more network data.
        if (count.error() == ParseError::insufficient_data) {
            return std::unexpected{ParseError::malformed_payload};
        }

        return std::unexpected{count.error()};
    }

    std::vector<BlockHeader> headers;

    headers.reserve(static_cast<std::size_t>(*count));

    for (std::uint64_t index = 0; index < *count; ++index) {
        auto header = parse_block_header(reader);

        if (!header) {
            return std::unexpected{header.error()};
        }

        // Each header is followed by a transaction count that is always zero in
        // a headers message; any other value is a protocol violation.
        const auto transaction_count = read_compact_size(reader);

        if (!transaction_count) {
            if (transaction_count.error() == ParseError::insufficient_data) {
                return std::unexpected{ParseError::malformed_payload};
            }

            return std::unexpected{transaction_count.error()};
        }

        if (*transaction_count != 0) {
            return std::unexpected{ParseError::malformed_payload};
        }

        headers.push_back(std::move(*header));
    }

    if (!reader.empty()) {
        return std::unexpected{ParseError::trailing_bytes};
    }

    return headers;
}
