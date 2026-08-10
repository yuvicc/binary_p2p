#pragma once

#include "message.h"
#include "util/writer.h"

// serialize the body of a "getheaders" (or "getblocks") message.
void serialize_getheaders_payload(
    ByteWriter& writer,
    const GetHeadersMessage& message
);
