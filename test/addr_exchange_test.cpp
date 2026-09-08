#define BOOST_TEST_MODULE AddrExchange
#include <boost/test/unit_test.hpp>

#include "addressv2.h"
#include "message.h"
#include "net/addr_exchange.h"
#include "net/peer.h"
#include "net/tcp_connection.h"
#include "network_magic.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>
#include <thread>
#include <vector>

#include <sys/socket.h>

namespace boost::test_tools::tt_detail {
template<>
struct print_log_value<SyncErrorCode> {
    void operator()(std::ostream& os, SyncErrorCode e) const
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

namespace {

struct SocketPair {
    TcpConnection a;
    TcpConnection b;
};

SocketPair make_socket_pair()
{
    int fds[2] = {-1, -1};
    BOOST_REQUIRE(::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
    return SocketPair{TcpConnection{fds[0]}, TcpConnection{fds[1]}};
}

AddressV2Entry torv3_entry()
{
    return AddressV2Entry{
        .timestamp = 1'700'000'001U,
        .services = 1,
        .network_id = AddressV2Network::torv3,
        .address = std::vector<std::byte>(32, std::byte{0xC3}),
        .port = 8333,
    };
}

// The legacy addr form of 203.0.113.7:8333, IPv4-mapped into 16 bytes.
AddressEntry legacy_ipv4_entry()
{
    std::array<std::byte, 16> ip{};
    ip[10] = std::byte{0xff};
    ip[11] = std::byte{0xff};
    ip[12] = std::byte{203};
    ip[13] = std::byte{0};
    ip[14] = std::byte{113};
    ip[15] = std::byte{7};

    return AddressEntry{
        .timestamp = 1'700'000'000U,
        .address = NetworkAddress{
            .services = 0x0409,
            .ip_address = ip,
            .port = 8333,
        },
    };
}

} // namespace

// A BIP155-capable peer answers the getaddr with an addrv2.
BOOST_AUTO_TEST_CASE(addrv2_reply_is_returned)
{
    auto pair = make_socket_pair();
    Peer client{std::move(pair.a), NetworkMagic::mainnet};
    Peer remote{std::move(pair.b), NetworkMagic::mainnet};

    std::string requested;

    std::thread server([&] {
        auto raw = remote.receive();
        if (raw) {
            requested = raw->header.command_name();
        }
        (void)remote.send(
            "addrv2",
            AddrV2Message{.addresses = {torv3_entry()}}
        );
    });

    const auto addresses = request_addresses(client);

    server.join();

    // There is no getaddrv2: the plain getaddr asks for either format.
    BOOST_TEST(requested == "getaddr");

    BOOST_REQUIRE(addresses.has_value());
    BOOST_REQUIRE(addresses->size() == 1);
    BOOST_TEST((*addresses)[0].network_id == AddressV2Network::torv3);
    BOOST_TEST((*addresses)[0].address.size() == 32U);
    BOOST_TEST((*addresses)[0].port == 8333U);
}

// A peer that never negotiated addrv2 replies with a legacy addr, which is
// widened to the addrv2 representation.
BOOST_AUTO_TEST_CASE(legacy_addr_reply_is_converted)
{
    auto pair = make_socket_pair();
    Peer client{std::move(pair.a), NetworkMagic::mainnet};
    Peer remote{std::move(pair.b), NetworkMagic::mainnet};

    std::thread server([&] {
        (void)remote.receive();
        (void)remote.send(
            "addr",
            AddrMessage{.addresses = {legacy_ipv4_entry()}}
        );
    });

    const auto addresses = request_addresses(client);

    server.join();

    BOOST_REQUIRE(addresses.has_value());
    BOOST_REQUIRE(addresses->size() == 1);

    const auto& entry = (*addresses)[0];
    BOOST_TEST(entry.network_id == AddressV2Network::ipv4);
    BOOST_REQUIRE(entry.address.size() == 4);
    BOOST_TEST(entry.address[0] == std::byte{203});
    BOOST_TEST(entry.address[3] == std::byte{7});
    BOOST_TEST(entry.port == 8333U);
    BOOST_TEST(entry.services == 0x0409U);
    BOOST_TEST(is_usable_address(entry));
}

// Unrelated traffic before the reply is skipped, and a ping is answered so the
// connection survives the wait.
BOOST_AUTO_TEST_CASE(unrelated_messages_are_skipped_and_pings_answered)
{
    auto pair = make_socket_pair();
    Peer client{std::move(pair.a), NetworkMagic::mainnet};
    Peer remote{std::move(pair.b), NetworkMagic::mainnet};

    std::string pong_command;

    std::thread server([&] {
        (void)remote.receive(); // the getaddr

        (void)remote.send("inv", InvMessage{});
        (void)remote.send("ping", PingMessage{.nonce = 0xABCD});

        if (auto raw = remote.receive()) {
            pong_command = raw->header.command_name();
        }

        (void)remote.send("addrv2", AddrV2Message{});
    });

    const auto addresses = request_addresses(client);

    server.join();

    BOOST_TEST(pong_command == "pong");
    BOOST_REQUIRE(addresses.has_value());
    BOOST_TEST(addresses->empty());
}

// A peer that hangs up without answering surfaces as a closed connection.
BOOST_AUTO_TEST_CASE(closed_connection_is_reported)
{
    auto pair = make_socket_pair();
    Peer client{std::move(pair.a), NetworkMagic::mainnet};

    std::thread server([&] {
        // Take the getaddr, then hang up without answering. The peer must own
        // the connection inside the thread so that it closes on the way out,
        // while the client is already blocked in its receive.
        Peer remote{std::move(pair.b), NetworkMagic::mainnet};
        (void)remote.receive();
    });

    const auto addresses = request_addresses(client);

    server.join();

    BOOST_REQUIRE(!addresses.has_value());
    BOOST_TEST(addresses.error() == SyncErrorCode::connection_closed);
}

// A malformed addrv2 body is a protocol error, not an empty result.
BOOST_AUTO_TEST_CASE(malformed_reply_is_a_protocol_error)
{
    auto pair = make_socket_pair();
    Peer client{std::move(pair.a), NetworkMagic::mainnet};
    Peer remote{std::move(pair.b), NetworkMagic::mainnet};

    std::thread server([&] {
        (void)remote.receive();
        // A count of one with no entry bytes behind it.
        (void)remote.send(
            "addrv2",
            UnknownMessage{.payload = {std::byte{0x01}}}
        );
    });

    const auto addresses = request_addresses(client);

    server.join();

    BOOST_REQUIRE(!addresses.has_value());
    BOOST_TEST(addresses.error() == SyncErrorCode::protocol_error);
}
