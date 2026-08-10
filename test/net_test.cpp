#define BOOST_TEST_MODULE Net
#include <boost/test/unit_test.hpp>

#include "message.h"
#include "message_serializer.h"
#include "net/peer.h"
#include "net/tcp_connection.h"
#include "network_magic.h"
#include "payload_parser.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <span>
#include <thread>
#include <variant>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace boost::test_tools::tt_detail {
template<>
struct print_log_value<PeerErrorCode> {
    void operator()(std::ostream& os, PeerErrorCode e) const
    {
        os << static_cast<int>(e);
    }
};
}

namespace {

// A connected pair of local sockets, each wrapped in a TcpConnection.
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

} // namespace

BOOST_AUTO_TEST_CASE(tcp_connection_transfers_bytes)
{
    auto pair = make_socket_pair();

    const std::array<std::byte, 4> payload{
        std::byte{0xde}, std::byte{0xad}, std::byte{0xbe}, std::byte{0xef},
    };
    BOOST_REQUIRE(pair.a.send_all(payload).has_value());

    std::array<std::byte, 16> buffer{};
    const auto received = pair.b.receive(buffer);
    BOOST_REQUIRE(received.has_value());
    BOOST_REQUIRE(*received == 4);
    BOOST_TEST((std::span{buffer}.first(4)[0] == payload[0]));
    BOOST_TEST((std::span{buffer}.first(4)[3] == payload[3]));
}

BOOST_AUTO_TEST_CASE(receive_reports_peer_close)
{
    auto pair = make_socket_pair();
    pair.b.close(); // hang up one end

    std::array<std::byte, 16> buffer{};
    const auto received = pair.a.receive(buffer);
    BOOST_REQUIRE(received.has_value());
    BOOST_TEST(*received == 0); // 0 bytes == peer closed
}

// A message sent by one Peer is received whole by the other.
BOOST_AUTO_TEST_CASE(peer_exchanges_a_message)
{
    auto pair = make_socket_pair();
    Peer sender{std::move(pair.a), NetworkMagic::mainnet};
    Peer receiver{std::move(pair.b), NetworkMagic::mainnet};

    BOOST_REQUIRE(
        sender.send("ping", PingMessage{.nonce = 0xABCD}).has_value());

    const auto raw = receiver.receive();
    BOOST_REQUIRE(raw.has_value());
    BOOST_TEST(raw->header.command_name() == "ping");

    const auto message = parse_payload(*raw);
    BOOST_REQUIRE(message.has_value());
    BOOST_REQUIRE(std::holds_alternative<PingMessage>(message->payload));
    BOOST_TEST(std::get<PingMessage>(message->payload).nonce == 0xABCDU);
}

BOOST_AUTO_TEST_CASE(peer_receive_reports_connection_closed)
{
    auto pair = make_socket_pair();
    Peer receiver{std::move(pair.a), NetworkMagic::mainnet};
    pair.b.close(); // hang up before sending anything

    const auto raw = receiver.receive();
    BOOST_REQUIRE(!raw.has_value());
    BOOST_TEST(raw.error() == PeerErrorCode::connection_closed);
}

// End to end over a real loopback TCP connection: a background server accepts a
// connection and sends a verack; the client connects and receives it.
BOOST_AUTO_TEST_CASE(connect_over_loopback)
{
    const int listener = ::socket(AF_INET, SOCK_STREAM, 0);
    BOOST_REQUIRE(listener != -1);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0; // ask the OS for an ephemeral port

    BOOST_REQUIRE(::bind(listener,
                         reinterpret_cast<sockaddr*>(&addr),
                         sizeof(addr)) == 0);
    BOOST_REQUIRE(::listen(listener, 1) == 0);

    socklen_t addr_len = sizeof(addr);
    BOOST_REQUIRE(::getsockname(listener,
                               reinterpret_cast<sockaddr*>(&addr),
                               &addr_len) == 0);
    const std::uint16_t port = ntohs(addr.sin_port);

    std::thread server([listener] {
        const int conn = ::accept(listener, nullptr, nullptr);
        if (conn == -1) {
            return;
        }
        const auto frame = serialize_message(
            NetworkMagic::mainnet, "verack", VerackMessage{});
        (void)::send(conn, frame.data(), frame.size(), 0);
        ::close(conn);
    });

    auto client = TcpConnection::connect("127.0.0.1", port);
    BOOST_REQUIRE(client.has_value());

    Peer peer{std::move(*client), NetworkMagic::mainnet};
    const auto raw = peer.receive();
    BOOST_REQUIRE(raw.has_value());
    BOOST_TEST(raw->header.command_name() == "verack");

    server.join();
    ::close(listener);
}
