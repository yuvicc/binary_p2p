#pragma once

#include "message.h"
#include "util/writer.h"

// serialize the body of a "block" message.
void serialize_block_payload(
    ByteWriter& writer,
    const BlockMessage& message
);
