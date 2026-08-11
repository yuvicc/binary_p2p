#define BOOST_TEST_MODULE BlockLocator
#include <boost/test/unit_test.hpp>

#include "block_locator.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

using Hash = std::array<std::byte, 32>;

Hash hash_of(std::uint8_t fill)
{
    Hash hash{};
    hash.fill(std::byte{fill});
    return hash;
}

std::vector<Hash> chain_of(std::size_t n)
{
    std::vector<Hash> chain;
    for (std::size_t i = 0; i < n; ++i) {
        chain.push_back(hash_of(static_cast<std::uint8_t>(i)));
    }
    return chain;
}

} // namespace

BOOST_AUTO_TEST_CASE(empty_chain_yields_empty_locator)
{
    BOOST_TEST(build_block_locator({}).empty());
}

BOOST_AUTO_TEST_CASE(single_block_chain)
{
    const auto locator = build_block_locator(chain_of(1));
    BOOST_REQUIRE(locator.size() == 1);
    BOOST_TEST((locator.front() == hash_of(0)));
}

// A short chain is listed in full, newest first, ending at genesis.
BOOST_AUTO_TEST_CASE(short_chain_is_dense)
{
    const auto chain = chain_of(5);
    const auto locator = build_block_locator(chain);

    BOOST_REQUIRE(locator.size() == 5);
    BOOST_TEST((locator.front() == chain.back()));  // tip first
    BOOST_TEST((locator.back() == chain.front()));  // genesis last
    BOOST_TEST((locator[1] == hash_of(3)));
}

// A long chain steps back exponentially, so the locator is far shorter than the
// chain, still tip-first and genesis-last with genesis appearing once.
BOOST_AUTO_TEST_CASE(long_chain_is_sparse)
{
    const auto chain = chain_of(100);
    const auto locator = build_block_locator(chain);

    BOOST_TEST((locator.front() == chain.back()));
    BOOST_TEST((locator.back() == chain.front()));
    BOOST_TEST(locator.size() < chain.size());
    BOOST_TEST(locator.size() < 25u);

    std::size_t genesis_count = 0;
    for (const auto& hash : locator) {
        if (hash == chain.front()) {
            ++genesis_count;
        }
    }
    BOOST_TEST(genesis_count == 1u);

    // The 10 most recent hashes are listed densely (step of 1).
    for (std::size_t i = 0; i < 10; ++i) {
        BOOST_TEST((locator[i] == chain[chain.size() - 1 - i]));
    }
}
