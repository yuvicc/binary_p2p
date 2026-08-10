#include "block_parser.h"

#include "compact_size.h"
#include "headers_parser.h"
#include "util/reader.h"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

std::expected<BlockMessage, ParseError>
parse_block_payload(
    std::span<const std::byte> payload
)
{
    ByteReader reader{payload};

    auto header = parse_block_header(reader);

    if (!header) {
        return std::unexpected{header.error()};
    }

    const auto transaction_count = read_compact_size(reader);

    if (!transaction_count) {
        if (transaction_count.error() == ParseError::insufficient_data) {
            return std::unexpected{ParseError::malformed_payload};
        }

        return std::unexpected{transaction_count.error()};
    }

    const auto transactions = reader.unread_bytes();

    if (*transaction_count == 0 && !transactions.empty()) {
        return std::unexpected{ParseError::trailing_bytes};
    }

    if (*transaction_count > 0 && transactions.empty()) {
        return std::unexpected{ParseError::malformed_payload};
    }

    BlockMessage message;
    message.header = *header;
    message.transaction_count = *transaction_count;
    message.transactions.assign(transactions.begin(), transactions.end());

    return message;
}
