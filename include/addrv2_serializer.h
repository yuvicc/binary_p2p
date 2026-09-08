#pragma once

#include "addressv2.h"
#include "util/writer.h"

#include <vector>

// serialize an addrv2 payload
void serialize_addrv2_payload(
    ByteWriter& writer,
    const std::vector<AddressV2Entry>& addresses
);
