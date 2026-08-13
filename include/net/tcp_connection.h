#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>

enum class TcpErrorCode {
    resolve_failed,
    connect_failed,
    send_failed,
    receive_failed,
};

// TCP socket (POSIX).
class TcpConnection {
public:
    explicit TcpConnection(int fd) noexcept;

    ~TcpConnection();

    TcpConnection(TcpConnection&& other) noexcept;
    TcpConnection& operator=(TcpConnection&& other) noexcept;

    TcpConnection(const TcpConnection&) = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;

    // Resolve host name and open a connection to port.
    [[nodiscard]]
    static std::expected<TcpConnection, TcpErrorCode>
    connect(std::string_view host, std::uint16_t port);

    // Send every byte, retrying on partial writes.
    [[nodiscard]]
    std::expected<void, TcpErrorCode>
    send_all(std::span<const std::byte> data);

    // Read up to buffer.size() bytes. A return value of 0 means the peer closed
    // the connection.
    [[nodiscard]]
    std::expected<std::size_t, TcpErrorCode>
    receive(std::span<std::byte> buffer);

    [[nodiscard]] int native_handle() const noexcept { return m_fd; }

    void close() noexcept;

private:
    int m_fd{-1};
};
