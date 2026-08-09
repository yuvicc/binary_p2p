#pragma once

#include "util/writer.h"
#include "version_message.h"

// serialize version msg
void serialize_version_payload(
    ByteWriter& writer,
    const VersionMessage& message
);
