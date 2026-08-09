#define BOOST_TEST_MODULE CompactSize
#include <boost/test/unit_test.hpp>

#include "compact_size.h"
#include "util/reader.h"
#include "util/writer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <vector>

namespace boost::test_tools::tt_detail {
template<>
struct print_log_value<std::byte> {
    void operator()(std::ostream& os, std::byte b) const
    {
        os << "0x" << std::hex << std::to_integer<int>(b);
    }
};

template<>
struct print_log_value<ParseError> {
    void operator()(std::ostream& os, ParseError e) const
    {
        os << static_cast<int>(e);
    }
};
}

namespace {

std::vector<std::byte> encode(std::uint64_t value)
{
    ByteWriter writer;
    write_compact_size(writer, value);
    return writer.take();
}

} // namespace

BOOST_AUTO_TEST_CASE(single_byte_form)
{
    const auto bytes = encode(0x00);
    BOOST_REQUIRE(bytes.size() == 1);
    BOOST_TEST(bytes[0] == std::byte{0x00});

    const auto max_single = encode(0xfc);
    BOOST_REQUIRE(max_single.size() == 1);
    BOOST_TEST(max_single[0] == std::byte{0xfc});
}

BOOST_AUTO_TEST_CASE(three_byte_form)
{
    // 0xfd is the first value that needs the 0xfd prefix + u16.
    const auto bytes = encode(0xfd);
    BOOST_REQUIRE(bytes.size() == 3);
    BOOST_TEST(bytes[0] == std::byte{0xfd});
    BOOST_TEST(bytes[1] == std::byte{0xfd});
    BOOST_TEST(bytes[2] == std::byte{0x00});

    const auto max_three = encode(0xffff);
    BOOST_REQUIRE(max_three.size() == 3);
    BOOST_TEST(max_three[0] == std::byte{0xfd});
    BOOST_TEST(max_three[1] == std::byte{0xff});
    BOOST_TEST(max_three[2] == std::byte{0xff});
}

BOOST_AUTO_TEST_CASE(five_byte_form)
{
    const auto bytes = encode(0x1'0000);
    BOOST_REQUIRE(bytes.size() == 5);
    BOOST_TEST(bytes[0] == std::byte{0xfe});
    BOOST_TEST(bytes[1] == std::byte{0x00});
    BOOST_TEST(bytes[2] == std::byte{0x00});
    BOOST_TEST(bytes[3] == std::byte{0x01});
    BOOST_TEST(bytes[4] == std::byte{0x00});
}

BOOST_AUTO_TEST_CASE(nine_byte_form)
{
    const auto bytes = encode(0x1'0000'0000ULL);
    BOOST_REQUIRE(bytes.size() == 9);
    BOOST_TEST(bytes[0] == std::byte{0xff});
    BOOST_TEST(bytes[5] == std::byte{0x01}); // 2^32 = LE 00 00 00 00 01 00 00 00
}

// Every encoded value must decode back to itself in canonical form.
BOOST_AUTO_TEST_CASE(round_trips_through_reader)
{
    const std::array<std::uint64_t, 9> values{
        0, 1, 0xfc, 0xfd, 0xff, 0xffff,
        0x1'0000, 0xffff'ffffULL, 0x1'0000'0000ULL,
    };

    for (const auto value : values) {
        const auto bytes = encode(value);
        ByteReader reader{bytes};

        const auto decoded = read_compact_size(reader);
        BOOST_REQUIRE(decoded.has_value());
        BOOST_TEST(*decoded == value);
        BOOST_TEST(reader.empty()); // consumed exactly the canonical encoding
    }
}
