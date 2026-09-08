#define BOOST_TEST_MODULE Handshake
#include <boost/test/unit_test.hpp>

#include "message.h"
#include "net/handshake.h"
#include "net/peer.h"
#include "net/tcp_connection.h"
#include "network_magic.h"
#include "version_message.h"

#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <thread>
#include <vector>

#include <sys/socket.h>

namespace boost::test_tools::tt_detail {
template<>
struct print_log_value<HandshakeErrorCode> {
    void operator()(std::ostream& os, HandshakeErrorCode e) const
    {
        os << static_cast<int>(e);
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

VersionMessage version_with_agent(std::string agent, std::int32_t height)
{
    return VersionMessage{
        .protocol_version = 70016,
        .services = 0x0409,
        .timestamp = 1'700'000'000,
        .nonce = 0x1234,
        .user_agent = std::move(agent),
        .start_height = height,
        .relay = true,
    };
}

} // namespace

// Both ends run the initiator handshake concurrently; each learns the other's
// advertised version.
BOOST_AUTO_TEST_CASE(symmetric_handshake_succeeds)
{
    auto pair = make_socket_pair();
    Peer peer_a{std::move(pair.a), NetworkMagic::mainnet};
    Peer peer_b{std::move(pair.b), NetworkMagic::mainnet};

    std::optional<std::expected<HandshakeResult, HandshakeErrorCode>> result_b;
    std::thread other([&] {
        result_b = perform_handshake(peer_b, version_with_agent("/b/", 200));
    });

    const auto result_a =
        perform_handshake(peer_a, version_with_agent("/a/", 100));

    other.join();

    BOOST_REQUIRE(result_a.has_value());
    BOOST_TEST(result_a->version.user_agent == "/b/");
    BOOST_TEST(result_a->version.start_height == 200);

    BOOST_REQUIRE(result_b.has_value());
    BOOST_REQUIRE(result_b->has_value());
    BOOST_TEST((*result_b)->version.user_agent == "/a/");
    BOOST_TEST((*result_b)->version.start_height == 100);

    // Both ends advertise 70016, so each offered addrv2 and saw the other's.
    BOOST_TEST(result_a->supports_addrv2);
    BOOST_TEST((*result_b)->supports_addrv2);
}

// A peer that interleaves negotiation messages before its verack still
// completes the handshake.
BOOST_AUTO_TEST_CASE(handshake_ignores_negotiation_messages)
{
    auto pair = make_socket_pair();
    Peer client{std::move(pair.a), NetworkMagic::mainnet};
    Peer remote{std::move(pair.b), NetworkMagic::mainnet};

    std::thread server([&] {
        // Mimic a modern peer: version, then feature signals, then verack.
        (void)remote.send("version", version_with_agent("/remote/", 555));
        (void)remote.send("wtxidrelay", UnknownMessage{});
        (void)remote.send("sendaddrv2", UnknownMessage{});
        (void)remote.send("sendheaders", UnknownMessage{});
        (void)remote.send("verack", VerackMessage{});
    });

    const auto result =
        perform_handshake(client, version_with_agent("/client/", 1));

    server.join();

    BOOST_REQUIRE(result.has_value());
    BOOST_TEST(result->version.user_agent == "/remote/");
    BOOST_TEST(result->version.start_height == 555);
    BOOST_TEST(result->supports_addrv2);
}

// A peer that never sends sendaddrv2 must not be reported as supporting it.
BOOST_AUTO_TEST_CASE(handshake_reports_no_addrv2_when_peer_stays_silent)
{
    auto pair = make_socket_pair();
    Peer client{std::move(pair.a), NetworkMagic::mainnet};
    Peer remote{std::move(pair.b), NetworkMagic::mainnet};

    std::thread server([&] {
        (void)remote.send("version", version_with_agent("/quiet/", 7));
        (void)remote.send("sendheaders", UnknownMessage{});
        (void)remote.send("verack", VerackMessage{});
    });

    const auto result =
        perform_handshake(client, version_with_agent("/client/", 1));

    server.join();

    BOOST_REQUIRE(result.has_value());
    BOOST_TEST(!result->supports_addrv2);
}

// BIP155 arrived in protocol version 70016. A peer older than that would treat
// an unknown sendaddrv2 command as a protocol violation, so we must not offer
// it one.
BOOST_AUTO_TEST_CASE(handshake_withholds_sendaddrv2_from_older_peers)
{
    auto pair = make_socket_pair();
    Peer client{std::move(pair.a), NetworkMagic::mainnet};
    Peer remote{std::move(pair.b), NetworkMagic::mainnet};

    std::vector<std::string> received;

    std::thread server([&] {
        auto old_version = version_with_agent("/old/", 3);
        old_version.protocol_version = addrv2_protocol_version - 1;

        (void)remote.send("version", old_version);
        (void)remote.send("verack", VerackMessage{});

        // Record what the client sent us, up to and including its verack.
        while (true) {
            auto raw = remote.receive();
            if (!raw) {
                break;
            }
            received.emplace_back(raw->header.command_name());
            if (received.back() == "verack") {
                break;
            }
        }
    });

    const auto result =
        perform_handshake(client, version_with_agent("/client/", 1));

    server.join();

    BOOST_REQUIRE(result.has_value());
    BOOST_TEST(!result->supports_addrv2);

    const std::vector<std::string> expected{"version", "verack"};
    BOOST_TEST((received == expected));
}

// If the peer's first message is not a version, the handshake is rejected.
BOOST_AUTO_TEST_CASE(handshake_rejects_non_version_first)
{
    auto pair = make_socket_pair();
    Peer client{std::move(pair.a), NetworkMagic::mainnet};
    Peer remote{std::move(pair.b), NetworkMagic::mainnet};

    std::thread server([&] {
        (void)remote.send("ping", PingMessage{.nonce = 1});
    });

    const auto result =
        perform_handshake(client, version_with_agent("/client/", 1));

    server.join();

    BOOST_REQUIRE(!result.has_value());
    BOOST_TEST(result.error() == HandshakeErrorCode::unexpected_message);
}
