// Connect to a Bitcoin node, perform the version handshake, and exchange a
// ping/pong to prove the link is live.
//
//   Usage: connect [host] [port] [network]
//   Defaults: 127.0.0.1 18444 regtest
//
// Start a regtest node first, e.g.:
//   bitcoind -regtest -listen=1 -port=18444

#include "message.h"
#include "net/handshake.h"
#include "net/peer.h"
#include "net/tcp_connection.h"
#include "network_magic.h"
#include "payload_parser.h"
#include "version_message.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>
#include <string_view>
#include <variant>

namespace {

std::string_view handshake_error_name(HandshakeErrorCode code)
{
    switch (code) {
    case HandshakeErrorCode::connection_closed: return "connection_closed";
    case HandshakeErrorCode::io_error:          return "io_error";
    case HandshakeErrorCode::protocol_error:    return "protocol_error";
    case HandshakeErrorCode::unexpected_message:return "unexpected_message";
    }
    return "unknown";
}

VersionMessage local_version()
{
    VersionMessage version;
    version.protocol_version = 70016;
    version.services = 0; // NODE_NONE: we are just a client here
    version.timestamp = static_cast<std::int64_t>(std::time(nullptr));
    version.nonce = 0x00C0FFEE'0BADF00DULL;
    version.user_agent = "/binary_p2p:0.1.0/";
    version.start_height = 0;
    version.relay = true;
    return version;
}

} // namespace

int main(int argc, char** argv)
{
    const std::string host = argc > 1 ? argv[1] : "127.0.0.1";
    const std::uint16_t port = argc > 2
        ? static_cast<std::uint16_t>(std::atoi(argv[2]))
        : 18444;
    const std::string network = argc > 3 ? argv[3] : "regtest";

    const auto magic = network == "mainnet"
        ? NetworkMagic::mainnet
        : NetworkMagic::regtest;

    std::cout << "connecting to " << host << ':' << port
              << " (" << network << ")\n";

    auto connection = TcpConnection::connect(host, port);
    if (!connection) {
        std::cerr << "connect failed (is the node listening?)\n";
        return 1;
    }
    std::cout << "tcp connected\n";

    Peer peer{std::move(*connection), magic};

    const auto peer_version = perform_handshake(peer, local_version());
    if (!peer_version) {
        std::cerr << "handshake failed: "
                  << handshake_error_name(peer_version.error()) << '\n';
        return 1;
    }

    std::cout << "handshake complete\n";
    std::cout << "  peer version : " << peer_version->protocol_version << '\n';
    std::cout << "  user agent   : " << peer_version->user_agent << '\n';
    std::cout << "  services     : 0x" << std::hex << peer_version->services
              << std::dec << '\n';
    std::cout << "  start height : " << peer_version->start_height << '\n';

    // Liveness check: send a ping and wait for the matching pong, replying to
    // any ping the peer sends us in the meantime.
    const std::uint64_t ping_nonce = 0x1122'3344'5566'7788ULL;
    if (!peer.send("ping", PingMessage{.nonce = ping_nonce})) {
        std::cerr << "failed to send ping\n";
        return 1;
    }
    std::cout << "sent ping, waiting for pong...\n";

    for (int i = 0; i < 50; ++i) {
        auto raw = peer.receive();
        if (!raw) {
            std::cerr << "connection ended before pong\n";
            return 1;
        }

        const auto command = raw->header.command_name();
        std::cout << "  <- " << command << '\n';

        auto message = parse_payload(*raw);
        if (!message) {
            continue; // ignore anything we cannot parse
        }

        if (const auto* ping = std::get_if<PingMessage>(&message->payload)) {
            (void)peer.send("pong", PongMessage{.nonce = ping->nonce});
            continue;
        }

        if (const auto* pong = std::get_if<PongMessage>(&message->payload)) {
            if (pong->nonce == ping_nonce) {
                std::cout << "got our pong -- link is alive\n";
                return 0;
            }
        }
    }

    std::cerr << "did not receive our pong within 50 messages\n";
    return 1;
}
