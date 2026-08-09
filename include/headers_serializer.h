#pragma once

#include "block_header.h"
#include "util/writer.h"

#include <vector>

void serialize_block_header(ByteWriter& writer, const BlockHeader& header);

void serialize_headers_payload(
    ByteWriter& writer,
    const std::vector<BlockHeader>& headers
);
