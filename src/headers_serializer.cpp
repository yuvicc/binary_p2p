#include "headers_serializer.h"

#include "compact_size.h"

void serialize_block_header(ByteWriter& writer, const BlockHeader& header)
{
    writer.write_i32_le(header.version);
    writer.write_array(header.previous_block_hash);
    writer.write_array(header.merkle_root);
    writer.write_u32_le(header.timestamp);
    writer.write_u32_le(header.bits);
    writer.write_u32_le(header.nonce);
}

void serialize_headers_payload(
    ByteWriter& writer,
    const std::vector<BlockHeader>& headers
)
{
    write_compact_size(writer, headers.size());

    for (const auto& header : headers) {
        serialize_block_header(writer, header);

        write_compact_size(writer, 0);
    }
}
