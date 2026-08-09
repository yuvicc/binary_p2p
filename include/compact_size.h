#pragma once

#include "util/reader.h"
#include "util/writer.h"
#include "parse_error.h"

#include <cstdint>
#include <expected>
#include <limits>

[[nodiscard]]
std::expected<std::uint64_t, ParseError> read_compact_size(
    ByteReader& reader,
    std::uint64_t max = std::numeric_limits<std::uint64_t>::max()
) noexcept;

void write_compact_size(ByteWriter& writer, std::uint64_t value);






