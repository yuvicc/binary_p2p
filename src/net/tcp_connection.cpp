#include "net/tcp_connection.h"

#include <cerrno>
#include <string>
#include <utility>

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

TcpConnection::TcpConnection(int fd) noexcept
: m_fd{fd}
{ }

TcpConnection::~TcpConnection()
{
    close();
}

TcpConnection::TcpConnection(TcpConnection&& other) noexcept
: m_fd{std::exchange(other.m_fd, -1)}
{ }

TcpConnection& TcpConnection::operator=(TcpConnection&& other) noexcept
{
    if (this != &other) {
        close();
        m_fd = std::exchange(other.m_fd, -1);
    }
    return *this;
}

void TcpConnection::close() noexcept
{
    if (m_fd != -1) {
        ::close(m_fd);
        m_fd = -1;
    }
}

std::expected<TcpConnection, TcpErrorCode>
TcpConnection::connect(std::string_view host, std::uint16_t port)
{
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;     // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP

    const std::string host_str{host};
    const std::string port_str = std::to_string(port);

    addrinfo* candidates = nullptr;
    if (::getaddrinfo(host_str.c_str(), port_str.c_str(), &hints, &candidates)
        != 0) {
        return std::unexpected{TcpErrorCode::resolve_failed};
    }

    int fd = -1;
    for (addrinfo* rp = candidates; rp != nullptr; rp = rp->ai_next) {
        fd = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd == -1) {
            continue;
        }

        if (::connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            break; // connected
        }

        ::close(fd);
        fd = -1;
    }

    ::freeaddrinfo(candidates);

    if (fd == -1) {
        return std::unexpected{TcpErrorCode::connect_failed};
    }

    return TcpConnection{fd};
}

std::expected<void, TcpErrorCode>
TcpConnection::send_all(std::span<const std::byte> data)
{
    std::size_t sent = 0;

    while (sent < data.size()) {
        const auto written = ::send(
            m_fd,
            data.data() + sent,
            data.size() - sent,
            MSG_NOSIGNAL // report a broken pipe as an error, not a signal
        );

        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return std::unexpected{TcpErrorCode::send_failed};
        }

        sent += static_cast<std::size_t>(written);
    }

    return {};
}

std::expected<std::size_t, TcpErrorCode>
TcpConnection::receive(std::span<std::byte> buffer)
{
    while (true) {
        const auto received = ::recv(m_fd, buffer.data(), buffer.size(), 0);

        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            return std::unexpected{TcpErrorCode::receive_failed};
        }

        return static_cast<std::size_t>(received); // 0 == peer closed
    }
}
