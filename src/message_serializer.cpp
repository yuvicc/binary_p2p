#include "message_serializer.h"

#include "addr_serializer.h"
#include "addrv2_serializer.h"
#include "block_serializer.h"
#include "getheaders_serializer.h"
#include "headers_serializer.h"
#include "inventory_serializer.h"
#include "message_checksum.h"
#include "message_header.h"
#include "util/writer.h"
#include "version_serializer.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <variant>

std::vector<std::byte> serialize_payload(const MessagePayload& payload)
{
    ByteWriter writer;

    std::visit(
        [&writer](const auto& message) {
            using T = std::decay_t<decltype(message)>;

            if constexpr (std::is_same_v<T, VersionMessage>) {
                serialize_version_payload(writer, message);
            }
            else if constexpr (std::is_same_v<T, VerackMessage>) {
                // verack carries no payload
            }
            else if constexpr (std::is_same_v<T, PingMessage>) {
                writer.write_u64_le(message.nonce);
            }
            else if constexpr (std::is_same_v<T, PongMessage>) {
                writer.write_u64_le(message.nonce);
            }
            else if constexpr (std::is_same_v<T, InvMessage>) {
                serialize_inventory_payload(writer, message.inventory);
            }
            else if constexpr (std::is_same_v<T, GetdataMessage>) {
                serialize_inventory_payload(writer, message.inventory);
            }
            else if constexpr (std::is_same_v<T, AddrMessage>) {
                serialize_addr_payload(writer, message.addresses);
            }
            else if constexpr (std::is_same_v<T, GetAddrMessage>) {
                // getaddr carries no payload
            }
            else if constexpr (std::is_same_v<T, AddrV2Message>) {
                serialize_addrv2_payload(writer, message.addresses);
            }
            else if constexpr (std::is_same_v<T, SendAddrV2Message>) {
                // sendaddrv2 carries no payload
            }
            else if constexpr (std::is_same_v<T, HeadersMessage>) {
                serialize_headers_payload(writer, message.headers);
            }
            else if constexpr (std::is_same_v<T, GetHeadersMessage>) {
                serialize_getheaders_payload(writer, message);
            }
            else if constexpr (std::is_same_v<T, BlockMessage>) {
                serialize_block_payload(writer, message);
            }
            else if constexpr (std::is_same_v<T, UnknownMessage>) {
                writer.write_bytes(message.payload);
            }
        },
        payload
    );

    return writer.take();
}

std::vector<std::byte> serialize_message(
    std::array<std::byte, 4> magic,
    std::string_view command,
    const MessagePayload& payload
)
{
    const auto body = serialize_payload(payload);
    const auto checksum = calculate_message_checksum(body);

    ByteWriter writer{MessageHeader::encoded_size + body.size()};

    writer.write_array(magic);

    // The command is written as a fixed 12-byte field, zero-padded on the right.
    std::array<std::byte, MessageHeader::command_size> command_bytes{};
    for (std::size_t index = 0;
         index < command.size() && index < command_bytes.size();
         ++index) {
        command_bytes[index] =
            static_cast<std::byte>(static_cast<unsigned char>(command[index]));
    }
    writer.write_array(command_bytes);

    writer.write_u32_le(static_cast<std::uint32_t>(body.size()));
    writer.write_array(checksum);
    writer.write_bytes(body);

    return writer.take();
}

std::vector<std::byte> serialize_message(const Message& message)
{
    return serialize_message(
        message.header.magic,
        message.header.command_name(),
        message.payload
    );
}
