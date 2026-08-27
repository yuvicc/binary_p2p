#define BOOST_TEST_MODULE Addr
#include <boost/test/unit_test.hpp>

#include "addr_parser.h"
#include "addr_serializer.h"
#include "version_payload_fixture.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <vector>

namespace boost::test_tools::tt_detail {

template<>
struct print_log_value<ParseError> {
    void operator()(std::ostream& os, ParseError e) const
    {
        os << static_cast<int>(e);
    }
};

template<>
struct print_log_value<std::byte> {
    void operator()(std::ostream& os, std::byte b) const
    {
        os << "0x" << std::hex << std::to_integer<int>(b);
    }
};
}

using namespace test_support;

namespace {

// Encodes a canonical CompactSize.
void put_compact_size(std::vector<std::byte>& out, std::uint64_t value)
{
    if (value < 0xfd) {
        put_u8(out, static_cast<std::uint8_t>(value));
    } else if (value <= 0xffff) {
        put_u8(out, 0xfd);
        put_u8(out, static_cast<std::uint8_t>(value));
        put_u8(out, static_cast<std::uint8_t>(value >> 8));
    } else if (value <= 0xffff'ffff) {
        put_u8(out, 0xfe);
        put_le<std::uint32_t>(out, static_cast<std::uint32_t>(value));
    } else {
        put_u8(out, 0xff);
        put_le<std::uint64_t>(out, value);
    }
}

// The IPv4-mapped IPv6 form Bitcoin uses to carry an IPv4 address.
std::array<std::byte, 16> ipv4_mapped(
    std::uint8_t a,
    std::uint8_t b,
    std::uint8_t c,
    std::uint8_t d
)
{
    std::array<std::byte, 16> ip{};
    ip[10] = std::byte{0xff};
    ip[11] = std::byte{0xff};
    ip[12] = std::byte{a};
    ip[13] = std::byte{b};
    ip[14] = std::byte{c};
    ip[15] = std::byte{d};
    return ip;
}

// One addr entry: time(u32 LE) + services(u64 LE) + ip(16) + port(u16 BE).
void put_address_entry(
    std::vector<std::byte>& out,
    std::uint32_t timestamp,
    std::uint64_t services,
    const std::array<std::byte, 16>& ip,
    std::uint16_t port
)
{
    put_le<std::uint32_t>(out, timestamp);
    put_le<std::uint64_t>(out, services);
    for (const std::byte byte : ip) {
        put_u8(out, std::to_integer<std::uint8_t>(byte));
    }
    put_u16_be(out, port);
}

AddressEntry sample_entry(std::uint32_t timestamp, std::uint16_t port)
{
    return AddressEntry{
        .timestamp = timestamp,
        .address = NetworkAddress{
            .services = 0x0409,
            .ip_address = ipv4_mapped(203, 0, 113, 7),
            .port = port,
        },
    };
}

} // namespace

BOOST_AUTO_TEST_CASE(empty_address_list_succeeds)
{
    std::vector<std::byte> payload;
    put_compact_size(payload, 0);

    const auto result = parse_addr_payload(payload);

    BOOST_REQUIRE(result.has_value());
    BOOST_TEST(result->empty());
}

BOOST_AUTO_TEST_CASE(single_entry_is_parsed)
{
    std::vector<std::byte> payload;
    put_compact_size(payload, 1);
    put_address_entry(
        payload, 0x5F5E'0100U, 0x0409, ipv4_mapped(203, 0, 113, 7), 8333
    );

    const auto result = parse_addr_payload(payload);

    BOOST_REQUIRE(result.has_value());
    BOOST_REQUIRE(result->size() == 1);

    const auto& entry = (*result)[0];
    BOOST_TEST(entry.timestamp == 0x5F5E'0100U);
    BOOST_TEST(entry.address.services == 0x0409U);
    BOOST_TEST((entry.address.ip_address == ipv4_mapped(203, 0, 113, 7)));
    // The port is big-endian on the wire but native in the parsed struct.
    BOOST_TEST(entry.address.port == 8333U);
}

BOOST_AUTO_TEST_CASE(multiple_entries_are_parsed_in_order)
{
    std::vector<std::byte> payload;
    put_compact_size(payload, 3);
    put_address_entry(payload, 1, 0, ipv4_mapped(1, 1, 1, 1), 8333);
    put_address_entry(payload, 2, 1, ipv4_mapped(2, 2, 2, 2), 18333);
    put_address_entry(payload, 3, 2, ipv4_mapped(3, 3, 3, 3), 38333);

    const auto result = parse_addr_payload(payload);

    BOOST_REQUIRE(result.has_value());
    BOOST_REQUIRE(result->size() == 3);
    BOOST_TEST((*result)[0].timestamp == 1U);
    BOOST_TEST((*result)[1].timestamp == 2U);
    BOOST_TEST((*result)[2].timestamp == 3U);
    BOOST_TEST((*result)[1].address.port == 18333U);
    BOOST_TEST(((*result)[2].address.ip_address == ipv4_mapped(3, 3, 3, 3)));
}

BOOST_AUTO_TEST_CASE(maximum_count_is_accepted)
{
    std::vector<std::byte> payload;
    put_compact_size(payload, 1'000); // MAX_ADDR_TO_SEND
    for (int i = 0; i < 1'000; ++i) {
        put_address_entry(payload, 0, 0, ipv4_mapped(10, 0, 0, 1), 8333);
    }

    const auto result = parse_addr_payload(payload);

    BOOST_REQUIRE(result.has_value());
    BOOST_TEST(result->size() == 1'000U);
}

BOOST_AUTO_TEST_CASE(empty_payload_is_malformed)
{
    const std::vector<std::byte> payload;

    const auto result = parse_addr_payload(payload);

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::malformed_payload);
}

BOOST_AUTO_TEST_CASE(truncated_entry_is_malformed)
{
    std::vector<std::byte> payload;
    put_compact_size(payload, 1);
    put_address_entry(payload, 0, 0, ipv4_mapped(10, 0, 0, 1), 8333);
    payload.pop_back(); // drop one byte of the port

    const auto result = parse_addr_payload(payload);

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::malformed_payload);
}

BOOST_AUTO_TEST_CASE(fewer_entries_than_declared_is_malformed)
{
    std::vector<std::byte> payload;
    put_compact_size(payload, 2); // claims two, supplies one
    put_address_entry(payload, 0, 0, ipv4_mapped(10, 0, 0, 1), 8333);

    const auto result = parse_addr_payload(payload);

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::malformed_payload);
}

BOOST_AUTO_TEST_CASE(trailing_bytes_are_rejected)
{
    std::vector<std::byte> payload;
    put_compact_size(payload, 1);
    put_address_entry(payload, 0, 0, ipv4_mapped(10, 0, 0, 1), 8333);
    put_u8(payload, 0x99); // one extra byte beyond the declared entry

    const auto result = parse_addr_payload(payload);

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::trailing_bytes);
}

BOOST_AUTO_TEST_CASE(count_above_maximum_is_rejected)
{
    std::vector<std::byte> payload;
    put_compact_size(payload, 1'001); // MAX_ADDR_TO_SEND is 1000

    const auto result = parse_addr_payload(payload);

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::compact_size_too_large);
}

BOOST_AUTO_TEST_CASE(non_canonical_count_is_rejected)
{
    // Encode the value 1 using the three-byte 0xfd form instead of one byte.
    std::vector<std::byte> payload{
        std::byte{0xfd}, std::byte{0x01}, std::byte{0x00}
    };

    const auto result = parse_addr_payload(payload);

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::non_canonical_compact_size);
}

BOOST_AUTO_TEST_CASE(serializer_matches_the_wire_layout)
{
    ByteWriter writer;
    serialize_addr_payload(
        writer,
        {sample_entry(0x0102'0304U, 8333)}
    );
    const auto bytes = writer.take();

    std::vector<std::byte> expected;
    put_compact_size(expected, 1);
    put_address_entry(
        expected, 0x0102'0304U, 0x0409, ipv4_mapped(203, 0, 113, 7), 8333
    );

    BOOST_REQUIRE(bytes.size() == 1 + 4 + 8 + 16 + 2);
    BOOST_TEST((bytes == expected));
}

BOOST_AUTO_TEST_CASE(empty_list_serializes_to_a_single_zero_count)
{
    ByteWriter writer;
    serialize_addr_payload(writer, {});
    const auto bytes = writer.take();

    BOOST_REQUIRE(bytes.size() == 1);
    BOOST_TEST(bytes[0] == std::byte{0x00});
}

BOOST_AUTO_TEST_CASE(serialized_addresses_round_trip)
{
    const std::vector<AddressEntry> original{
        sample_entry(1'700'000'000U, 8333),
        sample_entry(1'700'000'001U, 18333),
        AddressEntry{
            .timestamp = 0xFFFF'FFFFU,
            .address = NetworkAddress{
                .services = 0xFFFF'FFFF'FFFF'FFFFULL,
                .ip_address = {},
                .port = 0xFFFFU,
            },
        },
    };

    ByteWriter writer;
    serialize_addr_payload(writer, original);
    const auto bytes = writer.take();

    const auto recovered = parse_addr_payload(bytes);

    BOOST_REQUIRE(recovered.has_value());
    BOOST_TEST((*recovered == original));
}
