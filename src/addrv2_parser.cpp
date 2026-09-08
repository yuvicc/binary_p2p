#include "addrv2_parser.h"

#include "compact_size.h"
#include "util/reader.h"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace {

// BIP155 keeps the addr limit of MAX_ADDR_TO_SEND entries.
constexpr std::uint64_t max_address_count = 1'000;

} // namespace

std::expected<std::vector<AddressV2Entry>, ParseError>
parse_addrv2_payload(
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

    std::vector<AddressV2Entry> addresses;

    // Safe to reserve: read_compact_size bounded the count by max_address_count.
    addresses.reserve(static_cast<std::size_t>(*count));

    for (std::uint64_t index = 0; index < *count; ++index) {
        const auto timestamp = reader.read_u32_le();

        if (!timestamp) {
            return std::unexpected{ParseError::malformed_payload};
        }

        // Unlike the legacy addr, addrv2 encodes the service bits as a
        // CompactSize so that a peer advertising few services sends few bytes.
        const auto services = read_compact_size(reader);

        if (!services) {
            if (services.error() == ParseError::insufficient_data) {
                return std::unexpected{ParseError::malformed_payload};
            }

            return std::unexpected{services.error()};
        }

        const auto network_id = reader.read_u8();

        if (!network_id) {
            return std::unexpected{ParseError::malformed_payload};
        }

        const auto address_size = read_compact_size(
            reader,
            AddressV2Entry::max_address_size
        );

        if (!address_size) {
            if (address_size.error() == ParseError::insufficient_data) {
                return std::unexpected{ParseError::malformed_payload};
            }

            return std::unexpected{address_size.error()};
        }

        const auto address = reader.read_bytes(
            static_cast<std::size_t>(*address_size)
        );

        if (!address) {
            return std::unexpected{ParseError::malformed_payload};
        }

        // Bitcoin ports are encoded in network byte order (big-endian).
        const auto port = reader.read_u16_be();

        if (!port) {
            return std::unexpected{ParseError::malformed_payload};
        }

        // An unknown network id, or a known one carrying the wrong number of
        // address bytes, is not a malformed message: BIP155 asks receivers to
        // ignore such entries so that new network types can be introduced
        // without breaking older peers. The entry is kept verbatim here and
        // is_usable_address() tells the caller which ones it can interpret.
        addresses.push_back(
            AddressV2Entry{
                .timestamp = *timestamp,
                .services = *services,
                .network_id = *network_id,
                .address = {address->begin(), address->end()},
                .port = *port,
            }
        );
    }

    if (!reader.empty()) {
        return std::unexpected{ParseError::trailing_bytes};
    }

    return addresses;
}
