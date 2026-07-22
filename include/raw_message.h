#pragma once

#include "message_header.h"

#include <cstddef>
#include <vector>

struct RawMessage {
    MessageHeader header;
    std::vector<std::byte> payload;
};

struct ParsedRawMessage {
    RawMessage message;
    std::size_t bytes_consumed{};
};

