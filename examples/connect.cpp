// Connect to a Bitcoin node, perform the version handshake, and sync its block
// headers from genesis using getheaders/headers rounds.
//
//   Usage: connect [host] [port] [network]
//   Defaults: 127.0.0.1 18444 regtest
//
// Start a regtest node and mine some blocks first, e.g.:
//   bitcoind -regtest -datadir=/tmp/regtest-datadir -listen=1 -port=18444 -daemon
//   ADDR=$(bitcoin-cli -regtest -datadir=/tmp/regtest-datadir getnewaddress)
//   bitcoin-cli -regtest -datadir=/tmp/regtest-datadir generatetoaddress 20 "$ADDR"

#include "block_hash.h"
#include "block_locator.h"
#include "inventory.h"
#include "message.h"
#include "net/block_download.h"
#include "net/handshake.h"
#include "net/header_sync.h"
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
#include <vector>

namespace {

using Hash = std::array<std::byte, 32>;

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

std::string to_display_hex(const Hash& hash)
{
    static const char digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(64);
    // Displayed hashes are the internal bytes in reverse.
    for (std::size_t i = hash.size(); i-- > 0;) {
        const auto value = std::to_integer<unsigned>(hash[i]);
        out.push_back(digits[value >> 4]);
        out.push_back(digits[value & 0x0f]);
    }
    return out;
}

Hash from_display_hex(const std::string& hex)
{
    Hash out{};
    for (std::size_t i = 0; i < 32; ++i) {
        const auto value = static_cast<std::uint8_t>(
            std::stoi(hex.substr(i * 2, 2), nullptr, 16));
        out[31 - i] = static_cast<std::byte>(value);
    }
    return out;
}

// The genesis header for each network. Its fields are fixed by the protocol.
BlockHeader genesis_for(const std::string& network)
{
    BlockHeader genesis{
        .version = 1,
        .previous_block_hash = {},
        .merkle_root = from_display_hex(
            "4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b"),
        .timestamp = 1296688602,
        .bits = 0x207fffff,
        .nonce = 2,
    };

    if (network == "mainnet") {
        genesis.timestamp = 1231006505;
        genesis.bits = 0x1d00ffff;
        genesis.nonce = 2083236893;
    }

    return genesis;
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
    std::cout << "  start height : " << peer_version->start_height << "\n\n";

    // Our known chain, genesis-first. We start with just the genesis block.
    std::vector<BlockHeader> chain{genesis_for(network)};
    std::vector<Hash> chain_hashes{block_hash(chain.front())};

    // Headers-first sync: repeatedly ask for the headers after our current tip.
    std::cout << "syncing headers...\n";
    while (true) {
        const auto locator = build_block_locator(chain_hashes);

        const auto headers = request_headers(peer, 70016, locator);
        if (!headers) {
            std::cerr << "getheaders failed (code "
                      << static_cast<int>(headers.error()) << ")\n";
            return 1;
        }

        if (headers->empty()) {
            break; // the peer has nothing after our tip
        }

        // Connect each header to our chain by its previous_block_hash link.
        for (const auto& header : *headers) {
            if (header.previous_block_hash != chain_hashes.back()) {
                std::cerr << "header does not connect to our tip at height "
                          << chain.size() - 1 << '\n';
                return 1;
            }
            chain_hashes.push_back(block_hash(header));
            chain.push_back(header);
        }

        std::cout << "  received " << headers->size()
                  << " headers, height now " << chain.size() - 1 << '\n';

        if (headers->size() < 2000) {
            break; // last (partial) batch: we have reached the tip
        }
    }

    const std::size_t tip_height = chain.size() - 1;
    std::cout << "\nsynced to height " << tip_height << '\n';
    std::cout << "tip hash : " << to_display_hex(chain_hashes.back()) << '\n';
    std::cout << "(compare with: bitcoin-cli -regtest getbestblockhash)\n";

    // Download the full block for every header we synced.
    if (tip_height > 0) {
        std::cout << "\ndownloading " << tip_height << " blocks...\n";
        for (std::size_t height = 1; height <= tip_height; ++height) {
            const auto block = request_block(peer, chain_hashes[height]);
            if (!block) {
                std::cerr << "block download failed at height " << height
                          << " (code " << static_cast<int>(block.error())
                          << ")\n";
                return 1;
            }

            // The downloaded block must hash to the header we requested.
            if (block_hash(block->header) != chain_hashes[height]) {
                std::cerr << "downloaded block hash mismatch at height "
                          << height << '\n';
                return 1;
            }

            std::cout << "  block " << height << "  txs="
                      << block->transaction_count << '\n';
        }
        std::cout << "all blocks downloaded and verified\n";
    }

    // Stay connected and react to new blocks as the node announces them. We did
    // not send sendheaders, so the node announces new blocks with an inv; we
    // fetch each announced block with getdata and connect it to our chain.
    std::cout << "\nlistening for new blocks "
                 "(mine more on the node to see them; Ctrl-C to stop)...\n";
    while (true) {
        auto raw = peer.receive();
        if (!raw) {
            std::cout << "connection closed\n";
            break;
        }

        auto message = parse_payload(std::move(*raw));
        if (!message) {
            continue;
        }

        if (const auto* ping = std::get_if<PingMessage>(&message->payload)) {
            (void)peer.send("pong", PongMessage{.nonce = ping->nonce});
            continue;
        }

        const auto* inv = std::get_if<InvMessage>(&message->payload);
        if (inv == nullptr) {
            continue; // only block announcements are of interest here
        }

        for (const auto& item : inv->inventory) {
            if (item.type != InventoryType::block) {
                continue; // ignore transaction announcements
            }

            const auto block = request_block(peer, item.hash);
            if (!block) {
                std::cerr << "failed to fetch announced block\n";
                continue;
            }

            if (block->header.previous_block_hash != chain_hashes.back()) {
                std::cout << "announced block does not extend our tip "
                             "(gap or reorg): "
                          << to_display_hex(item.hash) << '\n';
                continue;
            }

            chain_hashes.push_back(item.hash);
            chain.push_back(block->header);
            std::cout << "new block at height " << chain.size() - 1
                      << " : " << to_display_hex(item.hash)
                      << "  (txs=" << block->transaction_count << ")\n";
        }
    }

    return 0;
}
