#pragma once

#include "util/writer.h"
#include "version_message.h"

// serialize a single 26-byte network address; the "addr" message reuses this
// layout behind a timestamp
void serialize_network_address(
    ByteWriter& writer,
    const NetworkAddress& address
);

// serialize version msg
void serialize_version_payload(
    ByteWriter& writer,
    const VersionMessage& message
);
