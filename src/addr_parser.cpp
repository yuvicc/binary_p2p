#include "addr_parser.h"

#include "compact_size.h"
#include "util/reader.h"
#include "version_parser.h"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace {

// Bitcoin caps an addr message at MAX_ADDR_TO_SEND entries.
constexpr std::uint64_t max_address_count = 1'000;

} // namespace

std::expected<std::vector<AddressEntry>, ParseError>
parse_addr_payload(
    std::span<const std::byte> payload
)
{
    ByteReader reader{payload};

    const auto count = read_compact_size(reader, max_address_count);

    if (!count) {
        // The RawMessage already holds the complete declared payload, so a
        // truncated CompactSize means the payload is malformed rather than
        // waiting for more network data.
        if (count.error() == ParseError::insufficient_data) {
            return std::unexpected{ParseError::malformed_payload};
        }

        return std::unexpected{count.error()};
    }

    std::vector<AddressEntry> addresses;

    // Safe to reserve: read_compact_size bounded the count by max_address_count.
    addresses.reserve(static_cast<std::size_t>(*count));

    for (std::uint64_t index = 0; index < *count; ++index) {
        const auto timestamp = reader.read_u32_le();

        if (!timestamp) {
            return std::unexpected{ParseError::malformed_payload};
        }

        const auto address = parse_network_address(reader);

        if (!address) {
            return std::unexpected{address.error()};
        }

        addresses.push_back(
            AddressEntry{
                .timestamp = *timestamp,
                .address = *address,
            }
        );
    }

    if (!reader.empty()) {
        return std::unexpected{ParseError::trailing_bytes};
    }

    return addresses;
}
