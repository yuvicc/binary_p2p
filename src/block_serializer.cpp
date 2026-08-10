#include "block_serializer.h"

#include "compact_size.h"
#include "headers_serializer.h"

void serialize_block_payload(
    ByteWriter& writer,
    const BlockMessage& message
)
{
    serialize_block_header(writer, message.header);
    write_compact_size(writer, message.transaction_count);
    writer.write_bytes(message.transactions);
}
