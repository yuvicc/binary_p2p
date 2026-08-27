#include "version_serializer.h"

#include "compact_size.h"

#include <cstddef>
#include <cstdint>

void serialize_network_address(
    ByteWriter& writer,
    const NetworkAddress& address
)
{
    writer.write_u64_le(address.services);
    writer.write_array(address.ip_address);
    writer.write_u16_be(address.port);
}

void serialize_version_payload(
    ByteWriter& writer,
    const VersionMessage& message
)
{
    writer.write_i32_le(message.protocol_version);
    writer.write_u64_le(message.services);
    writer.write_i64_le(message.timestamp);

    serialize_network_address(writer, message.receiver_address);
    serialize_network_address(writer, message.sender_address);

    writer.write_u64_le(message.nonce);

    write_compact_size(writer, message.user_agent.size());
    for (const char ch : message.user_agent) {
        writer.write_u8(static_cast<std::uint8_t>(ch));
    }

    writer.write_i32_le(message.start_height);

    // The relay flag is optional; emit a byte only when it was present.
    if (message.relay.has_value()) {
        writer.write_u8(*message.relay ? 1 : 0);
    }
}
