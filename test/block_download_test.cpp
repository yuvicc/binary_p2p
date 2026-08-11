#define BOOST_TEST_MODULE BlockDownload
#include <boost/test/unit_test.hpp>

#include "inventory.h"
#include "message.h"
#include "net/block_download.h"
#include "net/peer.h"
#include "net/tcp_connection.h"
#include "network_magic.h"

#include <array>
#include <cstddef>
#include <cstdint>
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

BlockMessage sample_block()
{
    return BlockMessage{
        .header = BlockHeader{
            .version = 1,
            .previous_block_hash = {},
            .merkle_root = {},
            .timestamp = 1296688602,
            .bits = 0x207fffff,
            .nonce = 2,
        },
        .transaction_count = 1,
        .transactions = std::vector<std::byte>(64, std::byte{0xAB}),
    };
}

std::array<std::byte, 32> some_hash()
{
    std::array<std::byte, 32> hash{};
    hash.fill(std::byte{0x07});
    return hash;
}

} // namespace

// request_block sends a getdata and returns the peer's block reply.
BOOST_AUTO_TEST_CASE(returns_block_reply)
{
    auto pair = make_socket_pair();
    Peer client{std::move(pair.a), NetworkMagic::mainnet};
    Peer server{std::move(pair.b), NetworkMagic::mainnet};

    const auto block = sample_block();

    std::thread responder([&] {
        const auto request = server.receive();
        if (!request || request->header.command_name() != "getdata") {
            return;
        }
        (void)server.send("block", block);
    });

    const auto received = request_block(client, some_hash());

    responder.join();

    BOOST_REQUIRE(received.has_value());
    BOOST_TEST((*received == block));
}

// A ping arriving before the block is answered and does not disrupt the fetch.
BOOST_AUTO_TEST_CASE(answers_ping_while_waiting)
{
    auto pair = make_socket_pair();
    Peer client{std::move(pair.a), NetworkMagic::mainnet};
    Peer server{std::move(pair.b), NetworkMagic::mainnet};

    const auto block = sample_block();

    std::thread responder([&] {
        const auto request = server.receive();
        if (!request || request->header.command_name() != "getdata") {
            return;
        }
        (void)server.send("ping", PingMessage{.nonce = 0x99});
        (void)server.send("block", block);
        (void)server.receive(); // consume the client's pong
    });

    const auto received = request_block(client, some_hash());

    responder.join();

    BOOST_REQUIRE(received.has_value());
    BOOST_TEST((*received == block));
}

// A notfound reply surfaces as an error.
BOOST_AUTO_TEST_CASE(notfound_is_an_error)
{
    auto pair = make_socket_pair();
    Peer client{std::move(pair.a), NetworkMagic::mainnet};
    Peer server{std::move(pair.b), NetworkMagic::mainnet};

    std::thread responder([&] {
        const auto request = server.receive();
        if (!request || request->header.command_name() != "getdata") {
            return;
        }
        // notfound carries an inventory payload, same shape as getdata.
        (void)server.send(
            "notfound",
            GetdataMessage{
                .inventory = {
                    InventoryVector{.type = InventoryType::block,
                                    .hash = some_hash()},
                },
            });
    });

    const auto received = request_block(client, some_hash());

    responder.join();

    BOOST_REQUIRE(!received.has_value());
    BOOST_TEST(received.error() == SyncErrorCode::protocol_error);
}
