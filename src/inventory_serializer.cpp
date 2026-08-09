#include "inventory_serializer.h"

#include "compact_size.h"

void serialize_inventory_payload(
    ByteWriter& writer,
    const std::vector<InventoryVector>& inventory
)
{
    write_compact_size(writer, inventory.size());

    for (const auto& entry : inventory) {
        writer.write_u32_le(entry.type);
        writer.write_array(entry.hash);
    }
}
