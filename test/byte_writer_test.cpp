#define BOOST_TEST_MODULE ByteWriter
#include <boost/test/unit_test.hpp>

#include <util/reader.h>
#include <util/writer.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <ostream>

// std::byte has no operator<<, so Boost.Test cannot print it when a check fails.
namespace boost::test_tools::tt_detail {
template<>
struct print_log_value<std::byte> {
    void operator()(std::ostream& os, std::byte b) const
    {
        os << "0x" << std::hex << std::to_integer<int>(b);
    }
};
}

BOOST_AUTO_TEST_CASE(initial_state)
{
    const ByteWriter writer;

    BOOST_TEST(writer.size() == 0);
    BOOST_TEST(writer.empty());
}

BOOST_AUTO_TEST_CASE(write_u8)
{
    ByteWriter writer;
    writer.write_u8(0xFF);

    const auto bytes = writer.written_bytes();
    BOOST_REQUIRE(bytes.size() == 1);
    BOOST_TEST(bytes[0] == std::byte{0xFF});
    BOOST_TEST(writer.size() == 1);
    BOOST_TEST(!writer.empty());
}

BOOST_AUTO_TEST_CASE(u16_little_endian)
{
    ByteWriter writer;
    writer.write_u16_le(0x1234);

    const auto bytes = writer.written_bytes();
    BOOST_REQUIRE(bytes.size() == 2);
    BOOST_TEST(bytes[0] == std::byte{0x34});
    BOOST_TEST(bytes[1] == std::byte{0x12});
}

BOOST_AUTO_TEST_CASE(u16_big_endian)
{
    ByteWriter writer;
    writer.write_u16_be(0x1234);

    const auto bytes = writer.written_bytes();
    BOOST_REQUIRE(bytes.size() == 2);
    BOOST_TEST(bytes[0] == std::byte{0x12});
    BOOST_TEST(bytes[1] == std::byte{0x34});
}

BOOST_AUTO_TEST_CASE(u32_little_endian)
{
    ByteWriter writer;
    writer.write_u32_le(0x12345678U);

    const auto bytes = writer.written_bytes();
    BOOST_REQUIRE(bytes.size() == 4);
    BOOST_TEST(bytes[0] == std::byte{0x78});
    BOOST_TEST(bytes[1] == std::byte{0x56});
    BOOST_TEST(bytes[2] == std::byte{0x34});
    BOOST_TEST(bytes[3] == std::byte{0x12});
}

BOOST_AUTO_TEST_CASE(u64_little_endian)
{
    ByteWriter writer;
    writer.write_u64_le(0x0102030405060708ULL);

    const auto bytes = writer.written_bytes();
    BOOST_REQUIRE(bytes.size() == 8);
    BOOST_TEST(bytes[0] == std::byte{0x08});
    BOOST_TEST(bytes[7] == std::byte{0x01});
}

BOOST_AUTO_TEST_CASE(signed_negative_one)
{
    ByteWriter writer;
    writer.write_i32_le(-1);

    const auto bytes = writer.written_bytes();
    BOOST_REQUIRE(bytes.size() == 4);
    BOOST_TEST(bytes[0] == std::byte{0xFF});
    BOOST_TEST(bytes[1] == std::byte{0xFF});
    BOOST_TEST(bytes[2] == std::byte{0xFF});
    BOOST_TEST(bytes[3] == std::byte{0xFF});
}

BOOST_AUTO_TEST_CASE(write_bytes_and_array)
{
    ByteWriter writer;
    const std::array<std::byte, 3> head{
        std::byte{0x10}, std::byte{0x20}, std::byte{0x30},
    };
    writer.write_array(head);
    const std::array<std::byte, 2> tail{std::byte{0x40}, std::byte{0x50}};
    writer.write_bytes(tail);

    const auto bytes = writer.written_bytes();
    BOOST_REQUIRE(bytes.size() == 5);
    BOOST_TEST(bytes[0] == std::byte{0x10});
    BOOST_TEST(bytes[3] == std::byte{0x40});
    BOOST_TEST(bytes[4] == std::byte{0x50});
}

BOOST_AUTO_TEST_CASE(take_moves_buffer_out)
{
    ByteWriter writer;
    writer.write_u16_le(0xBEEF);

    const auto owned = writer.take();
    BOOST_REQUIRE(owned.size() == 2);
    BOOST_TEST(owned[0] == std::byte{0xEF});
    BOOST_TEST(owned[1] == std::byte{0xBE});
    BOOST_TEST(writer.empty()); // moved-from writer is drained
}

// Writer output fed straight back through the reader must recover the values.
BOOST_AUTO_TEST_CASE(round_trips_through_reader)
{
    ByteWriter writer;
    writer.write_u8(0x2A);
    writer.write_u16_le(0x1234);
    writer.write_u16_be(0x1234);
    writer.write_u32_le(0xDEADBEEFU);
    writer.write_i32_le(-12345);
    writer.write_u64_le(0x0102030405060708ULL);
    writer.write_i64_le(-1);

    const auto buffer = writer.take();
    ByteReader reader{buffer};

    BOOST_TEST(reader.read_u8().value() == 0x2A);
    BOOST_TEST(reader.read_u16_le().value() == 0x1234);
    BOOST_TEST(reader.read_u16_be().value() == 0x1234);
    BOOST_TEST(reader.read_u32_le().value() == 0xDEADBEEFU);
    BOOST_TEST(reader.read_i32_le().value() == -12345);
    BOOST_TEST(reader.read_u64_le().value() == 0x0102030405060708ULL);
    BOOST_TEST(reader.read_i64_le().value() == -1);
    BOOST_TEST(reader.empty());
}
