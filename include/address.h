#pragma once

#include "version_message.h"

#include <cstdint>

// One entry of an "addr" message: a NetworkAddress preceded by the time the
// sending peer last saw that address. 
struct AddressEntry {
    std::uint32_t timestamp{};
    NetworkAddress address{};

    friend bool operator==(
        const AddressEntry&,
        const AddressEntry&
    ) = default;
};
