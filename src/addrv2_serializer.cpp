#include "addrv2_serializer.h"

#include "compact_size.h"

void serialize_addrv2_payload(
    ByteWriter& writer,
    const std::vector<AddressV2Entry>& addresses
)
{
    write_compact_size(writer, addresses.size());

    for (const auto& entry : addresses) {
        writer.write_u32_le(entry.timestamp);
        write_compact_size(writer, entry.services);
        writer.write_u8(entry.network_id);
        write_compact_size(writer, entry.address.size());
        writer.write_bytes(entry.address);
        writer.write_u16_be(entry.port);
    }
}
