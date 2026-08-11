#include "block_locator.h"

std::vector<std::array<std::byte, 32>>
build_block_locator(const std::vector<std::array<std::byte, 32>>& chain_hashes)
{
    std::vector<std::array<std::byte, 32>> locator;

    if (chain_hashes.empty()) {
        return locator;
    }

    int step = 1;
    long index = static_cast<long>(chain_hashes.size()) - 1;

    while (index >= 0) {
        locator.push_back(chain_hashes[static_cast<std::size_t>(index)]);

        // Densely list the 10 most recent hashes, then double the gap.
        if (locator.size() >= 10) {
            step *= 2;
        }

        index -= step;
    }

    // The locator must always end at genesis; the loop may have stepped past it.
    if (locator.back() != chain_hashes.front()) {
        locator.push_back(chain_hashes.front());
    }

    return locator;
}
