#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <cerrno>
#include <cstring>
#include <string>
#include <fcntl.h>
#include <poll.h>

#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

namespace caesar {

class P2PTcpSocket {
public:
    P2PTcpSocket() = default;

    explicit P2PTcpSocket(int fd)
        : fd_(fd) {}

    ~P2PTcpSocket() {
        close();
    }

    P2PTcpSocket(const P2PTcpSocket&) = delete;
    P2PTcpSocket& operator=(const P2PTcpSocket&) = delete;

    P2PTcpSocket(P2PTcpSocket&& other) noexcept
        : fd_(other.fd_) {
        other.fd_ = -1;
    }

    P2PTcpSocket& operator=(P2PTcpSocket&& other) noexcept {
        if (this != &other) {
            close();
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }

    void listen_on(
        std::uint16_t port,
        const std::string& address = "0.0.0.0") {

        close();

        fd_ = ::socket(AF_INET, SOCK_STREAM, 0);

        if (fd_ < 0)
            throw std::runtime_error(
                "TCP socket creation failed");

        int reuse = 1;

        if (::setsockopt(
                fd_,
                SOL_SOCKET,
                SO_REUSEADDR,
                &reuse,
                sizeof(reuse)) < 0) {
            close();
            throw std::runtime_error(
                "TCP setsockopt failed");
        }

        sockaddr_in server{};
        server.sin_family = AF_INET;
        server.sin_port = htons(port);

        if (::inet_pton(
                AF_INET,
                address.c_str(),
                &server.sin_addr) != 1) {
            close();
            throw std::runtime_error(
                "Invalid bind IPv4 address");
        }

        if (::bind(
                fd_,
                reinterpret_cast<sockaddr*>(&server),
                sizeof(server)) < 0) {
            const int bind_errno = errno;
            close();
            throw std::runtime_error(
                std::string("TCP bind failed: ") +
                std::strerror(bind_errno));
        }

        if (::listen(fd_, 8) < 0) {
            close();
            throw std::runtime_error(
                "TCP listen failed");
        }

        const int flags =
            ::fcntl(fd_, F_GETFL, 0);

        if (flags < 0 ||
            ::fcntl(
                fd_,
                F_SETFL,
                flags | O_NONBLOCK) < 0) {
            close();
            throw std::runtime_error(
                "TCP listener non-blocking setup failed");
        }
    }

    P2PTcpSocket accept_connection() const {

        if (fd_ < 0)
            throw std::runtime_error(
                "TCP socket is closed");

        const int client_fd =
            ::accept(fd_, nullptr, nullptr);

        if (client_fd < 0)
            throw std::runtime_error(
                "TCP accept failed");

        return P2PTcpSocket(client_fd);
    }

    std::string peer_address() const {
        if (fd_ < 0)
            throw std::runtime_error(
                "TCP socket is closed");

        sockaddr_in peer{};
        socklen_t peer_len = sizeof(peer);

        if (::getpeername(
                fd_,
                reinterpret_cast<sockaddr*>(&peer),
                &peer_len) < 0) {
            throw std::runtime_error(
                std::string("TCP getpeername failed: ") +
                std::strerror(errno));
        }

        char address[INET_ADDRSTRLEN]{};

        if (::inet_ntop(
                AF_INET,
                &peer.sin_addr,
                address,
                sizeof(address)) == nullptr) {
            throw std::runtime_error(
                std::string("TCP address conversion failed: ") +
                std::strerror(errno));
        }

        return std::string(address);
    }

    std::uint16_t peer_port() const {
        if (fd_ < 0)
            throw std::runtime_error(
                "TCP socket is closed");

        sockaddr_in peer{};
        socklen_t peer_len = sizeof(peer);

        if (::getpeername(
                fd_,
                reinterpret_cast<sockaddr*>(&peer),
                &peer_len) < 0) {
            throw std::runtime_error(
                std::string("TCP getpeername failed: ") +
                std::strerror(errno));
        }

        return ntohs(peer.sin_port);
    }

    void receive_all(
        std::uint8_t* data,
        std::size_t size) const {

        if (fd_ < 0)
            throw std::runtime_error(
                "TCP socket is closed");

        std::size_t received = 0;

        while (received < size) {

            const ssize_t result =
                ::recv(
                    fd_,
                    data + received,
                    size - received,
                    0);

            if (result <= 0)
                throw std::runtime_error(
                    "TCP receive failed");

            received +=
                static_cast<std::size_t>(result);
        }
    }

    void connect_to(
        const std::string& address,
        std::uint16_t port,
        std::uint32_t timeout_ms = 5000) {

        close();

        fd_ = ::socket(
            AF_INET,
            SOCK_STREAM,
            0);

        if (fd_ < 0)
            throw std::runtime_error(
                "TCP socket creation failed");

        sockaddr_in server{};
        server.sin_family = AF_INET;
        server.sin_port = htons(port);

        if (::inet_pton(
                AF_INET,
                address.c_str(),
                &server.sin_addr) != 1) {
            close();
            throw std::runtime_error(
                "Invalid IPv4 address");
        }

        const int flags = ::fcntl(
            fd_,
            F_GETFL,
            0);

        if (flags < 0) {
            close();
            throw std::runtime_error(
                "TCP failed to get socket flags");
        }

        if (::fcntl(
                fd_,
                F_SETFL,
                flags | O_NONBLOCK) < 0) {
            close();
            throw std::runtime_error(
                "TCP failed to enable non-blocking mode");
        }

        const int result = ::connect(
            fd_,
            reinterpret_cast<sockaddr*>(&server),
            sizeof(server));

        if (result == 0) {
            if (::fcntl(fd_, F_SETFL, flags) < 0) {
                close();
                throw std::runtime_error(
                    "TCP failed to restore socket mode");
            }
            return;
        }

        if (errno != EINPROGRESS) {
            const std::string error =
                std::string("TCP connection failed: ") +
                std::strerror(errno);
            close();
            throw std::runtime_error(error);
        }

        pollfd pfd{};
        pfd.fd = fd_;
        pfd.events = POLLOUT;

        const int poll_result = ::poll(
            &pfd,
            1,
            static_cast<int>(
                timeout_ms > 2147483647U
                    ? 2147483647U
                    : timeout_ms));

        if (poll_result == 0) {
            close();
            throw std::runtime_error(
                "TCP connection timed out");
        }

        if (poll_result < 0) {
            const std::string error =
                std::string("TCP connection wait failed: ") +
                std::strerror(errno);
            close();
            throw std::runtime_error(error);
        }

        if ((pfd.revents & (POLLOUT | POLLERR | POLLHUP)) == 0) {
            close();
            throw std::runtime_error(
                "TCP connection failed: unexpected poll event");
        }

        int socket_error = 0;
        socklen_t socket_error_size =
            sizeof(socket_error);

        if (::getsockopt(
                fd_,
                SOL_SOCKET,
                SO_ERROR,
                &socket_error,
                &socket_error_size) < 0) {
            const std::string error =
                std::string("TCP connection status failed: ") +
                std::strerror(errno);
            close();
            throw std::runtime_error(error);
        }

        if (socket_error != 0) {
            const std::string error =
                std::string("TCP connection failed: ") +
                std::strerror(socket_error);
            close();
            throw std::runtime_error(error);
        }

        if (::fcntl(fd_, F_SETFL, flags) < 0) {
            close();
            throw std::runtime_error(
                "TCP failed to restore socket mode");
        }
    }

    void send_all(
        const std::uint8_t* data,
        std::size_t size) const {

        if (fd_ < 0)
            throw std::runtime_error(
                "TCP socket is closed");

        std::size_t sent = 0;

        while (sent < size) {

            const ssize_t result =
                ::send(
                    fd_,
                    data + sent,
                    size - sent,
                    MSG_NOSIGNAL);

            if (result <= 0)
                throw std::runtime_error(
                    "TCP send failed");

            sent +=
                static_cast<std::size_t>(result);
        }
    }

    void set_timeouts(std::uint32_t timeout_ms) const {
        if (fd_ < 0)
            throw std::runtime_error("Invalid TCP socket");

        timeval tv{};
        tv.tv_sec = static_cast<time_t>(timeout_ms / 1000);
        tv.tv_usec =
            static_cast<suseconds_t>((timeout_ms % 1000) * 1000);

        if (::setsockopt(
                fd_,
                SOL_SOCKET,
                SO_RCVTIMEO,
                &tv,
                sizeof(tv)) < 0) {
            throw std::runtime_error(
                "Failed to set receive timeout");
        }

        if (::setsockopt(
                fd_,
                SOL_SOCKET,
                SO_SNDTIMEO,
                &tv,
                sizeof(tv)) < 0) {
            throw std::runtime_error(
                "Failed to set send timeout");
        }
    }

    void clear_timeouts() const {
        if (fd_ < 0)
            throw std::runtime_error("Invalid TCP socket");

        timeval tv{};
        if (::setsockopt(
                fd_,
                SOL_SOCKET,
                SO_RCVTIMEO,
                &tv,
                sizeof(tv)) < 0) {
            throw std::runtime_error(
                "Failed to clear receive timeout");
        }

        if (::setsockopt(
                fd_,
                SOL_SOCKET,
                SO_SNDTIMEO,
                &tv,
                sizeof(tv)) < 0) {
            throw std::runtime_error(
                "Failed to clear send timeout");
        }
    }

    bool valid() const noexcept {
        return fd_ >= 0;
    }

    int native_handle() const noexcept {
        return fd_;
    }

    void close() noexcept {
        if (fd_ >= 0) {
            ::shutdown(fd_, SHUT_RDWR);
            ::close(fd_);
            fd_ = -1;
        }
    }

private:
    int fd_{-1};
};

}
