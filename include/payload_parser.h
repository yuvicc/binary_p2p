#pragma once

#include "message.h"
#include "parse_error.h"
#include "raw_message.h"

#include <expected>

[[nodiscard]]
std::expected<Message, ParseError>
parse_payload(RawMessage raw_message);
