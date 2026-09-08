#define BOOST_TEST_MODULE AddrV2
#include <boost/test/unit_test.hpp>

#include "addrv2_parser.h"
#include "addrv2_serializer.h"
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

std::vector<std::byte> filled_address(std::size_t size, std::uint8_t fill)
{
    return std::vector<std::byte>(size, std::byte{fill});
}

// One addrv2 entry: time(u32 LE), services(CompactSize), networkID(u8),
// addr(CompactSize-prefixed bytes), port(u16 BE).
void put_addrv2_entry(
    std::vector<std::byte>& out,
    std::uint32_t timestamp,
    std::uint64_t services,
    std::uint8_t network_id,
    const std::vector<std::byte>& address,
    std::uint16_t port
)
{
    put_le<std::uint32_t>(out, timestamp);
    put_compact_size(out, services);
    put_u8(out, network_id);
    put_compact_size(out, address.size());
    for (const std::byte byte : address) {
        put_u8(out, std::to_integer<std::uint8_t>(byte));
    }
    put_u16_be(out, port);
}

AddressV2Entry ipv4_entry(std::uint8_t last_octet, std::uint16_t port)
{
    return AddressV2Entry{
        .timestamp = 1'700'000'000U,
        .services = 0x0409,
        .network_id = AddressV2Network::ipv4,
        .address = {
            std::byte{203}, std::byte{0}, std::byte{113}, std::byte{last_octet}
        },
        .port = port,
    };
}

} // namespace

BOOST_AUTO_TEST_CASE(empty_address_list_succeeds)
{
    std::vector<std::byte> payload;
    put_compact_size(payload, 0);

    const auto result = parse_addrv2_payload(payload);

    BOOST_REQUIRE(result.has_value());
    BOOST_TEST(result->empty());
}

BOOST_AUTO_TEST_CASE(ipv4_entry_is_parsed)
{
    std::vector<std::byte> payload;
    put_compact_size(payload, 1);
    put_addrv2_entry(
        payload,
        0x5F5E'0100U,
        0x0409,
        AddressV2Network::ipv4,
        {std::byte{203}, std::byte{0}, std::byte{113}, std::byte{7}},
        8333
    );

    const auto result = parse_addrv2_payload(payload);

    BOOST_REQUIRE(result.has_value());
    BOOST_REQUIRE(result->size() == 1);

    const auto& entry = (*result)[0];
    BOOST_TEST(entry.timestamp == 0x5F5E'0100U);
    BOOST_TEST(entry.services == 0x0409U);
    BOOST_TEST(entry.network_id == AddressV2Network::ipv4);
    BOOST_REQUIRE(entry.address.size() == 4);
    BOOST_TEST(entry.address[0] == std::byte{203});
    BOOST_TEST(entry.address[3] == std::byte{7});
    // The port is big-endian on the wire but native in the parsed struct.
    BOOST_TEST(entry.port == 8333U);
    BOOST_TEST(is_usable_address(entry));
}

// The service bits are a CompactSize here, not the fixed uint64 of a legacy
// addr, so a large value takes the nine-byte form.
BOOST_AUTO_TEST_CASE(large_services_use_the_nine_byte_compact_size)
{
    std::vector<std::byte> payload;
    put_compact_size(payload, 1);
    put_addrv2_entry(
        payload,
        1,
        0x0102'0304'0506'0708ULL,
        AddressV2Network::ipv6,
        filled_address(16, 0xAB),
        8333
    );

    const auto result = parse_addrv2_payload(payload);

    BOOST_REQUIRE(result.has_value());
    BOOST_REQUIRE(result->size() == 1);
    BOOST_TEST((*result)[0].services == 0x0102'0304'0506'0708ULL);
}

BOOST_AUTO_TEST_CASE(every_known_network_is_accepted)
{
    struct Case {
        std::uint8_t network_id;
        std::size_t size;
    };

    const Case cases[]{
        {AddressV2Network::ipv4, 4},
        {AddressV2Network::ipv6, 16},
        {AddressV2Network::torv2, 10},
        {AddressV2Network::torv3, 32},
        {AddressV2Network::i2p, 32},
        {AddressV2Network::cjdns, 16},
    };

    for (const auto& test_case : cases) {
        std::vector<std::byte> payload;
        put_compact_size(payload, 1);
        put_addrv2_entry(
            payload,
            1,
            1,
            test_case.network_id,
            filled_address(test_case.size, 0x5A),
            8333
        );

        const auto result = parse_addrv2_payload(payload);

        BOOST_REQUIRE(result.has_value());
        BOOST_REQUIRE(result->size() == 1);
        BOOST_TEST((*result)[0].address.size() == test_case.size);
        BOOST_TEST(is_usable_address((*result)[0]));
    }
}

// BIP155 asks receivers to ignore address types they do not know rather than
// reject the message, so a new network id must parse and survive intact.
BOOST_AUTO_TEST_CASE(unknown_network_is_kept_but_not_usable)
{
    std::vector<std::byte> payload;
    put_compact_size(payload, 2);
    put_addrv2_entry(payload, 1, 1, 0x99, filled_address(20, 0x11), 8333);
    put_addrv2_entry(
        payload,
        2,
        1,
        AddressV2Network::ipv4,
        {std::byte{10}, std::byte{0}, std::byte{0}, std::byte{1}},
        8333
    );

    const auto result = parse_addrv2_payload(payload);

    BOOST_REQUIRE(result.has_value());
    BOOST_REQUIRE(result->size() == 2);

    // The unknown entry is preserved verbatim...
    BOOST_TEST((*result)[0].network_id == 0x99);
    BOOST_TEST((*result)[0].address.size() == 20U);
    BOOST_TEST(!is_usable_address((*result)[0]));

    // ...and does not stop the entry after it from being parsed.
    BOOST_TEST(is_usable_address((*result)[1]));
    BOOST_TEST((*result)[1].timestamp == 2U);
}

// Likewise a known network carrying the wrong number of address bytes is
// ignorable, not malformed.
BOOST_AUTO_TEST_CASE(known_network_with_wrong_size_is_kept_but_not_usable)
{
    std::vector<std::byte> payload;
    put_compact_size(payload, 1);
    // ipv4 must be 4 bytes; this one is 5.
    put_addrv2_entry(
        payload, 1, 1, AddressV2Network::ipv4, filled_address(5, 0x01), 8333
    );

    const auto result = parse_addrv2_payload(payload);

    BOOST_REQUIRE(result.has_value());
    BOOST_REQUIRE(result->size() == 1);
    BOOST_TEST(!is_usable_address((*result)[0]));
}

BOOST_AUTO_TEST_CASE(maximum_address_size_is_accepted)
{
    std::vector<std::byte> payload;
    put_compact_size(payload, 1);
    put_addrv2_entry(
        payload, 1, 1, 0x77, filled_address(512, 0x01), 8333
    );

    const auto result = parse_addrv2_payload(payload);

    BOOST_REQUIRE(result.has_value());
    BOOST_TEST((*result)[0].address.size() == 512U);
}

BOOST_AUTO_TEST_CASE(oversized_address_is_rejected)
{
    std::vector<std::byte> payload;
    put_compact_size(payload, 1);
    put_addrv2_entry(
        payload, 1, 1, 0x77, filled_address(513, 0x01), 8333
    );

    const auto result = parse_addrv2_payload(payload);

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::compact_size_too_large);
}

BOOST_AUTO_TEST_CASE(empty_payload_is_malformed)
{
    const std::vector<std::byte> payload;

    const auto result = parse_addrv2_payload(payload);

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::malformed_payload);
}

BOOST_AUTO_TEST_CASE(truncated_entry_is_malformed)
{
    std::vector<std::byte> payload;
    put_compact_size(payload, 1);
    put_addrv2_entry(
        payload,
        1,
        1,
        AddressV2Network::ipv4,
        {std::byte{10}, std::byte{0}, std::byte{0}, std::byte{1}},
        8333
    );
    payload.pop_back(); // drop one byte of the port

    const auto result = parse_addrv2_payload(payload);

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::malformed_payload);
}

BOOST_AUTO_TEST_CASE(address_shorter_than_declared_is_malformed)
{
    std::vector<std::byte> payload;
    put_compact_size(payload, 1);
    put_le<std::uint32_t>(payload, 1);          // timestamp
    put_compact_size(payload, 1);               // services
    put_u8(payload, AddressV2Network::ipv6);    // network id
    put_compact_size(payload, 16);              // claims 16 address bytes
    put_u8(payload, 0x01);                      // supplies one

    const auto result = parse_addrv2_payload(payload);

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::malformed_payload);
}

BOOST_AUTO_TEST_CASE(trailing_bytes_are_rejected)
{
    std::vector<std::byte> payload;
    put_compact_size(payload, 1);
    put_addrv2_entry(
        payload,
        1,
        1,
        AddressV2Network::ipv4,
        {std::byte{10}, std::byte{0}, std::byte{0}, std::byte{1}},
        8333
    );
    put_u8(payload, 0x99); // one extra byte beyond the declared entry

    const auto result = parse_addrv2_payload(payload);

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::trailing_bytes);
}

BOOST_AUTO_TEST_CASE(count_above_maximum_is_rejected)
{
    std::vector<std::byte> payload;
    put_compact_size(payload, 1'001); // MAX_ADDR_TO_SEND is 1000

    const auto result = parse_addrv2_payload(payload);

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::compact_size_too_large);
}

BOOST_AUTO_TEST_CASE(non_canonical_services_are_rejected)
{
    std::vector<std::byte> payload;
    put_compact_size(payload, 1);
    put_le<std::uint32_t>(payload, 1);
    // Encode the service value 1 using the three-byte 0xfd form.
    put_u8(payload, 0xfd);
    put_u8(payload, 0x01);
    put_u8(payload, 0x00);

    const auto result = parse_addrv2_payload(payload);

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == ParseError::non_canonical_compact_size);
}

BOOST_AUTO_TEST_CASE(serializer_matches_the_wire_layout)
{
    ByteWriter writer;
    serialize_addrv2_payload(writer, {ipv4_entry(7, 8333)});
    const auto bytes = writer.take();

    std::vector<std::byte> expected;
    put_compact_size(expected, 1);
    put_addrv2_entry(
        expected,
        1'700'000'000U,
        0x0409,
        AddressV2Network::ipv4,
        {std::byte{203}, std::byte{0}, std::byte{113}, std::byte{7}},
        8333
    );

    // count(1) + time(4) + services(3, the 0xfd form) + net(1) + size(1)
    // + addr(4) + port(2)
    BOOST_REQUIRE(bytes.size() == 16);
    BOOST_TEST((bytes == expected));
}

BOOST_AUTO_TEST_CASE(empty_list_serializes_to_a_single_zero_count)
{
    ByteWriter writer;
    serialize_addrv2_payload(writer, {});
    const auto bytes = writer.take();

    BOOST_REQUIRE(bytes.size() == 1);
    BOOST_TEST(bytes[0] == std::byte{0x00});
}

BOOST_AUTO_TEST_CASE(serialized_addresses_round_trip)
{
    const std::vector<AddressV2Entry> original{
        ipv4_entry(7, 8333),
        AddressV2Entry{
            .timestamp = 1'700'000'001U,
            .services = 0xFFFF'FFFF'FFFF'FFFFULL,
            .network_id = AddressV2Network::torv3,
            .address = filled_address(32, 0xC3),
            .port = 0xFFFFU,
        },
        AddressV2Entry{
            .timestamp = 0,
            .services = 0,
            .network_id = 0x99, // an id this build does not know
            .address = filled_address(7, 0x42),
            .port = 1,
        },
    };

    ByteWriter writer;
    serialize_addrv2_payload(writer, original);
    const auto bytes = writer.take();

    const auto recovered = parse_addrv2_payload(bytes);

    BOOST_REQUIRE(recovered.has_value());
    BOOST_TEST((*recovered == original));
}

BOOST_AUTO_TEST_CASE(legacy_ipv4_mapped_address_converts_to_ipv4)
{
    std::array<std::byte, 16> ip{};
    ip[10] = std::byte{0xff};
    ip[11] = std::byte{0xff};
    ip[12] = std::byte{203};
    ip[13] = std::byte{0};
    ip[14] = std::byte{113};
    ip[15] = std::byte{7};

    const AddressEntry legacy{
        .timestamp = 1'700'000'000U,
        .address = NetworkAddress{
            .services = 0x0409,
            .ip_address = ip,
            .port = 8333,
        },
    };

    const auto converted = to_addressv2(legacy);

    BOOST_TEST(converted.network_id == AddressV2Network::ipv4);
    BOOST_REQUIRE(converted.address.size() == 4);
    BOOST_TEST(converted.address[0] == std::byte{203});
    BOOST_TEST(converted.address[3] == std::byte{7});
    BOOST_TEST(converted.timestamp == 1'700'000'000U);
    BOOST_TEST(converted.services == 0x0409U);
    BOOST_TEST(converted.port == 8333U);
    BOOST_TEST(is_usable_address(converted));
}

BOOST_AUTO_TEST_CASE(legacy_ipv6_address_converts_to_ipv6)
{
    std::array<std::byte, 16> ip{};
    ip.fill(std::byte{0x20});

    const AddressEntry legacy{
        .timestamp = 5,
        .address = NetworkAddress{.ip_address = ip, .port = 8333},
    };

    const auto converted = to_addressv2(legacy);

    BOOST_TEST(converted.network_id == AddressV2Network::ipv6);
    BOOST_REQUIRE(converted.address.size() == 16);
    BOOST_TEST(converted.address[0] == std::byte{0x20});
    BOOST_TEST(is_usable_address(converted));
}

// An all-zero legacy address shares the ten leading zero bytes of the
// IPv4-mapped prefix but not the 0xffff marker, so it stays IPv6.
BOOST_AUTO_TEST_CASE(legacy_unspecified_address_stays_ipv6)
{
    const auto converted = to_addressv2(AddressEntry{});

    BOOST_TEST(converted.network_id == AddressV2Network::ipv6);
    BOOST_TEST(converted.address.size() == 16U);
}
