#define BOOST_TEST_MODULE ByteReader
#include <boost/test/unit_test.hpp>

#include <util/reader.h>

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

template<>
struct print_log_value<ByteReaderErrorCode> {
    void operator()(std::ostream& os, ByteReaderErrorCode c) const
    {
        os << static_cast<int>(c);
    }
};
} 

BOOST_AUTO_TEST_CASE(initial_state)
{
    const std::array<std::byte, 4> bytes{
        std::byte{0x01}, std::byte{0x02}, std::byte{0x03}, std::byte{0x04},
    };

    const ByteReader reader{bytes};

    BOOST_TEST(reader.size() == 4);
    BOOST_TEST(reader.position() == 0);
    BOOST_TEST(reader.remaining() == 4);
    BOOST_TEST(!reader.empty());
}

BOOST_AUTO_TEST_CASE(read_u8)
{
    const std::array<std::byte, 1> bytes{std::byte{0xFF}};
    ByteReader reader{bytes};

    const auto value = reader.read_u8();

    BOOST_REQUIRE(value.has_value());
    BOOST_TEST(*value == 255);
    BOOST_TEST(reader.position() == 1);
    BOOST_TEST(reader.remaining() == 0);
    BOOST_TEST(reader.empty());
}

BOOST_AUTO_TEST_CASE(u16_little_endian)
{
    const std::array<std::byte, 2> bytes{std::byte{0x34}, std::byte{0x12}};
    ByteReader reader{bytes};

    const auto value = reader.read_u16_le();

    BOOST_REQUIRE(value.has_value());
    BOOST_TEST(*value == 0x1234);
}

BOOST_AUTO_TEST_CASE(u16_big_endian)
{
    const std::array<std::byte, 2> bytes{std::byte{0x12}, std::byte{0x34}};
    ByteReader reader{bytes};

    const auto value = reader.read_u16_be();

    BOOST_REQUIRE(value.has_value());
    BOOST_TEST(*value == 0x1234);
}

BOOST_AUTO_TEST_CASE(u32_little_endian)
{
    const std::array<std::byte, 4> bytes{
        std::byte{0x78}, std::byte{0x56}, std::byte{0x34}, std::byte{0x12},
    };
    ByteReader reader{bytes};

    const auto value = reader.read_u32_le();

    BOOST_REQUIRE(value.has_value());
    BOOST_TEST(*value == 0x12345678U);
}

BOOST_AUTO_TEST_CASE(u64_little_endian)
{
    const std::array<std::byte, 8> bytes{
        std::byte{0x08}, std::byte{0x07}, std::byte{0x06}, std::byte{0x05},
        std::byte{0x04}, std::byte{0x03}, std::byte{0x02}, std::byte{0x01},
    };
    ByteReader reader{bytes};

    const auto value = reader.read_u64_le();

    BOOST_REQUIRE(value.has_value());
    BOOST_TEST(*value == 0x0102030405060708ULL);
}

BOOST_AUTO_TEST_CASE(failed_read_does_not_advance)
{
    const std::array<std::byte, 2> bytes{std::byte{0x01}, std::byte{0x02}};
    ByteReader reader{bytes};

    const auto position_before = reader.position();
    const auto result = reader.read_u32_le();

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(reader.position() == position_before);
    BOOST_TEST(reader.remaining() == 2);
    BOOST_TEST(result.error().code == ByteReaderErrorCode::insufficient_data);
    BOOST_TEST(result.error().requested == 4);
    BOOST_TEST(result.error().available == 2);
}

BOOST_AUTO_TEST_CASE(read_array)
{
    const std::array<std::byte, 6> bytes{
        std::byte{0xFA}, std::byte{0xBF}, std::byte{0xB5},
        std::byte{0xDA}, std::byte{0x01}, std::byte{0x02},
    };
    ByteReader reader{bytes};

    const auto magic = reader.read_array<4>();

    BOOST_REQUIRE(magic.has_value());
    BOOST_TEST((*magic)[0] == std::byte{0xFA});
    BOOST_TEST((*magic)[1] == std::byte{0xBF});
    BOOST_TEST((*magic)[2] == std::byte{0xB5});
    BOOST_TEST((*magic)[3] == std::byte{0xDA});
    BOOST_TEST(reader.position() == 4);
    BOOST_TEST(reader.remaining() == 2);
}

BOOST_AUTO_TEST_CASE(read_bytes)
{
    const std::array<std::byte, 4> bytes{
        std::byte{0x10}, std::byte{0x20}, std::byte{0x30}, std::byte{0x40},
    };
    ByteReader reader{bytes};

    const auto result = reader.read_bytes(2);

    BOOST_REQUIRE(result.has_value());
    BOOST_TEST(result->size() == 2);
    BOOST_TEST((*result)[0] == std::byte{0x10});
    BOOST_TEST((*result)[1] == std::byte{0x20});
    BOOST_TEST(reader.position() == 2);
    BOOST_TEST(reader.remaining() == 2);
}

BOOST_AUTO_TEST_CASE(zero_byte_read)
{
    const std::array<std::byte, 1> bytes{std::byte{0x42}};
    ByteReader reader{bytes};

    const auto result = reader.read_bytes(0);

    BOOST_REQUIRE(result.has_value());
    BOOST_TEST(result->empty());
    BOOST_TEST(reader.position() == 0);
}

BOOST_AUTO_TEST_CASE(signed_value)
{
    const std::array<std::byte, 4> bytes{
        std::byte{0xFF}, std::byte{0xFF}, std::byte{0xFF}, std::byte{0xFF},
    };
    ByteReader reader{bytes};

    const auto value = reader.read_i32_le();

    BOOST_REQUIRE(value.has_value());
    BOOST_TEST(*value == -1);
}
