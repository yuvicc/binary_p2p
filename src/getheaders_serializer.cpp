#include "getheaders_serializer.h"

#include "compact_size.h"

void serialize_getheaders_payload(
    ByteWriter& writer,
    const GetHeadersMessage& message
)
{
    writer.write_u32_le(message.protocol_version);

    write_compact_size(writer, message.block_locator_hashes.size());
    for (const auto& hash : message.block_locator_hashes) {
        writer.write_array(hash);
    }

    writer.write_array(message.hash_stop);
}
