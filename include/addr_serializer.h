#pragma once

#include "address.h"
#include "util/writer.h"

#include <vector>

// serialize an addr payload
void serialize_addr_payload(
    ByteWriter& writer,
    const std::vector<AddressEntry>& addresses
);
