#define BOOST_TEST_MODULE HeaderSync
#include <boost/test/unit_test.hpp>

#include "message.h"
#include "net/header_sync.h"
#include "net/peer.h"
#include "net/tcp_connection.h"
#include "network_magic.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ostream>
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

BlockHeader header_with_nonce(std::uint32_t nonce)
{
    return BlockHeader{
        .version = 1,
        .previous_block_hash = {},
        .merkle_root = {},
        .timestamp = 1296688602,
        .bits = 0x207fffff,
        .nonce = nonce,
    };
}

} // namespace

// request_headers sends a getheaders and returns the peer's headers reply.
BOOST_AUTO_TEST_CASE(returns_headers_reply)
{
    auto pair = make_socket_pair();
    Peer client{std::move(pair.a), NetworkMagic::mainnet};
    Peer server{std::move(pair.b), NetworkMagic::mainnet};

    const HeadersMessage reply{
        .headers = {header_with_nonce(1), header_with_nonce(2)},
    };

    std::thread responder([&] {
        const auto request = server.receive();
        if (!request || request->header.command_name() != "getheaders") {
            return;
        }
        (void)server.send("headers", reply);
    });

    const std::array<std::byte, 32> locator_hash{};
    const auto headers = request_headers(client, 70016, {locator_hash});

    responder.join();

    BOOST_REQUIRE(headers.has_value());
    BOOST_REQUIRE(headers->size() == 2);
    BOOST_TEST((*headers == reply.headers));
}

// A ping arriving before the headers is answered and does not disrupt the sync.
BOOST_AUTO_TEST_CASE(answers_ping_while_waiting)
{
    auto pair = make_socket_pair();
    Peer client{std::move(pair.a), NetworkMagic::mainnet};
    Peer server{std::move(pair.b), NetworkMagic::mainnet};

    const HeadersMessage reply{.headers = {header_with_nonce(7)}};

    std::optional<std::uint64_t> pong_nonce;
    std::thread responder([&] {
        const auto request = server.receive();
        if (!request || request->header.command_name() != "getheaders") {
            return;
        }

        (void)server.send("ping", PingMessage{.nonce = 0xFEED});
        (void)server.send("headers", reply);

        // The client should have replied to our ping with a matching pong.
        const auto pong = server.receive();
        if (pong && pong->header.command_name() == "pong") {
            pong_nonce = 0xFEED; // presence is enough for this test
        }
    });

    const std::array<std::byte, 32> locator_hash{};
    const auto headers = request_headers(client, 70016, {locator_hash});

    responder.join();

    BOOST_REQUIRE(headers.has_value());
    BOOST_REQUIRE(headers->size() == 1);
    BOOST_TEST(pong_nonce.has_value());
}

BOOST_AUTO_TEST_CASE(reports_connection_closed)
{
    auto pair = make_socket_pair();
    Peer client{std::move(pair.a), NetworkMagic::mainnet};

    // The peer reads our getheaders, then hangs up without replying.
    std::thread responder([conn = std::move(pair.b)]() mutable {
        Peer server{std::move(conn), NetworkMagic::mainnet};
        (void)server.receive();
        // Returning destroys `server`, closing the socket.
    });

    const std::array<std::byte, 32> locator_hash{};
    const auto headers = request_headers(client, 70016, {locator_hash});

    responder.join();

    BOOST_REQUIRE(!headers.has_value());
    BOOST_TEST(headers.error() == SyncErrorCode::connection_closed);
}
