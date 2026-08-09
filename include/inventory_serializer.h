#pragma once

#include "inventory.h"
#include "util/writer.h"

#include <vector>

// serialize an inv/getdata payload
void serialize_inventory_payload(
    ByteWriter& writer,
    const std::vector<InventoryVector>& inventory
);
