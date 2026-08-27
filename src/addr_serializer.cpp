#include "addr_serializer.h"

#include "compact_size.h"
#include "version_serializer.h"

void serialize_addr_payload(
    ByteWriter& writer,
    const std::vector<AddressEntry>& addresses
)
{
    write_compact_size(writer, addresses.size());

    for (const auto& entry : addresses) {
        writer.write_u32_le(entry.timestamp);
        serialize_network_address(writer, entry.address);
    }
}
