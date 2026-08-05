#include "inventory_parser.h"

#include "compact_size.h"
#include "util/reader.h"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace {

// Bitcoin caps an inv/getdata message at 50k entries.
constexpr std::uint64_t max_inventory_count = 50'000;

} // namespace

std::expected<std::vector<InventoryVector>, ParseError>
parse_inventory_payload(
    std::span<const std::byte> payload
)
{
    ByteReader reader{payload};

    const auto count = read_compact_size(reader, max_inventory_count);

    if (!count) {
        // The RawMessage already holds the complete declared payload, so a
        // truncated CompactSize means the payload is malformed rather than
        // waiting for more network data.
        if (count.error() == ParseError::insufficient_data) {
            return std::unexpected{ParseError::malformed_payload};
        }

        return std::unexpected{count.error()};
    }

    std::vector<InventoryVector> inventory;

    // Safe to reserve: read_compact_size bounded the count by max_inventory_count.
    inventory.reserve(static_cast<std::size_t>(*count));

    for (std::uint64_t index = 0; index < *count; ++index) {
        const auto type = reader.read_u32_le();

        if (!type) {
            return std::unexpected{ParseError::malformed_payload};
        }

        const auto hash = reader.read_array<32>();

        if (!hash) {
            return std::unexpected{ParseError::malformed_payload};
        }

        inventory.push_back(
            InventoryVector{
                .type = *type,
                .hash = *hash,
            }
        );
    }

    if (!reader.empty()) {
        return std::unexpected{ParseError::trailing_bytes};
    }

    return inventory;
}
