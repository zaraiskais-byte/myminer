#pragma once

#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

namespace caesar {

/*
 * TCP socket wrapper.
 *
 * The underlying file descriptor is stored in std::atomic<int> so
 * that close() may be called from one thread (the main thread
 * during shutdown) while another thread (the accept loop) is
 * concurrently executing ::accept(). Before this change fd_ was a
 * plain int, and ThreadSanitizer reported a data race between the
 * write in close() and the read in accept_connection():
 *
 *   Write of size 4 in P2PTcpSocket::close()          (main thread)
 *   Read  of size 4 in P2PTcpSocket::accept_connection() (accept thread)
 *
 * Making fd_ atomic removes the C++ memory-model violation without
 * changing the shutdown ordering: close() still runs before join(),
 * so ::accept() is still woken up by ::shutdown(). The accept loop
 * catches the resulting exception, sees running_ == false, and
 * exits cleanly.
 */
class P2PTcpSocket {
   public:
    P2PTcpSocket() = default;

    explicit P2PTcpSocket(int fd) : fd_(fd) {
    }

    ~P2PTcpSocket() {
        close();
    }

    P2PTcpSocket(const P2PTcpSocket&) = delete;
    P2PTcpSocket& operator=(const P2PTcpSocket&) = delete;

    P2PTcpSocket(P2PTcpSocket&& other) noexcept : fd_(other.fd_.exchange(-1)) {
    }

    P2PTcpSocket& operator=(P2PTcpSocket&& other) noexcept {
        if (this != &other) {
            close();
            fd_.store(other.fd_.exchange(-1));
        }
        return *this;
    }

    void listen_on(std::uint16_t port, const std::string& address = "0.0.0.0") {
        close();

        const int new_fd = ::socket(AF_INET, SOCK_STREAM, 0);

        if (new_fd < 0)
            throw std::runtime_error("TCP socket creation failed");

        fd_.store(new_fd);

        int reuse = 1;

        if (::setsockopt(new_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
            close();
            throw std::runtime_error("TCP setsockopt failed");
        }

        sockaddr_in server{};
        server.sin_family = AF_INET;
        server.sin_port = htons(port);

        if (::inet_pton(AF_INET, address.c_str(), &server.sin_addr) != 1) {
            close();
            throw std::runtime_error("Invalid bind IPv4 address");
        }

        if (::bind(new_fd, reinterpret_cast<sockaddr*>(&server), sizeof(server)) < 0) {
            close();
            throw std::runtime_error(std::string("TCP bind failed: ") + std::strerror(errno));
        }

        if (::listen(new_fd, 8) < 0) {
            close();
            throw std::runtime_error("TCP listen failed");
        }
    }

    P2PTcpSocket accept_connection() const {
        const int current = fd_.load();

        if (current < 0)
            throw std::runtime_error("TCP socket is closed");

        const int client_fd = ::accept(current, nullptr, nullptr);

        if (client_fd < 0)
            throw std::runtime_error("TCP accept failed");

        return P2PTcpSocket(client_fd);
    }

    std::string peer_address() const {
        const int current = fd_.load();

        if (current < 0)
            throw std::runtime_error("TCP socket is closed");

        sockaddr_in peer{};
        socklen_t peer_len = sizeof(peer);

        if (::getpeername(current, reinterpret_cast<sockaddr*>(&peer), &peer_len) < 0) {
            throw std::runtime_error(std::string("TCP getpeername failed: ") +
                                     std::strerror(errno));
        }

        char address[INET_ADDRSTRLEN]{};

        if (::inet_ntop(AF_INET, &peer.sin_addr, address, sizeof(address)) == nullptr) {
            throw std::runtime_error(std::string("TCP address conversion failed: ") +
                                     std::strerror(errno));
        }

        return std::string(address);
    }

    std::uint16_t peer_port() const {
        const int current = fd_.load();

        if (current < 0)
            throw std::runtime_error("TCP socket is closed");

        sockaddr_in peer{};
        socklen_t peer_len = sizeof(peer);

        if (::getpeername(current, reinterpret_cast<sockaddr*>(&peer), &peer_len) < 0) {
            throw std::runtime_error(std::string("TCP getpeername failed: ") +
                                     std::strerror(errno));
        }

        return ntohs(peer.sin_port);
    }

    void receive_all(std::uint8_t* data, std::size_t size) const {
        const int current = fd_.load();

        if (current < 0)
            throw std::runtime_error("TCP socket is closed");

        std::size_t received = 0;

        while (received < size) {
            const ssize_t result = ::recv(current, data + received, size - received, 0);

            if (result <= 0)
                throw std::runtime_error("TCP receive failed");

            received += static_cast<std::size_t>(result);
        }
    }

    void connect_to(const std::string& address, std::uint16_t port) {
        close();

        const int new_fd = ::socket(AF_INET, SOCK_STREAM, 0);

        if (new_fd < 0)
            throw std::runtime_error("TCP socket creation failed");

        fd_.store(new_fd);

        sockaddr_in server{};
        server.sin_family = AF_INET;
        server.sin_port = htons(port);

        if (::inet_pton(AF_INET, address.c_str(), &server.sin_addr) != 1) {
            close();
            throw std::runtime_error("Invalid IPv4 address");
        }

        if (::connect(new_fd, reinterpret_cast<sockaddr*>(&server), sizeof(server)) < 0) {
            close();
            throw std::runtime_error("TCP connection failed");
        }
    }

    void send_all(const std::uint8_t* data, std::size_t size) const {
        const int current = fd_.load();

        if (current < 0)
            throw std::runtime_error("TCP socket is closed");

        std::size_t sent = 0;

        while (sent < size) {
            const ssize_t result = ::send(current, data + sent, size - sent, MSG_NOSIGNAL);

            if (result <= 0)
                throw std::runtime_error("TCP send failed");

            sent += static_cast<std::size_t>(result);
        }
    }

    void set_timeouts(std::uint32_t timeout_ms) const {
        const int current = fd_.load();

        if (current < 0)
            throw std::runtime_error("Invalid TCP socket");

        timeval tv{};
        tv.tv_sec = static_cast<time_t>(timeout_ms / 1000);
        tv.tv_usec = static_cast<suseconds_t>((timeout_ms % 1000) * 1000);

        if (::setsockopt(current, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
            throw std::runtime_error("Failed to set receive timeout");
        }

        if (::setsockopt(current, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
            throw std::runtime_error("Failed to set send timeout");
        }
    }

    bool valid() const noexcept {
        return fd_.load() >= 0;
    }

    int native_handle() const noexcept {
        return fd_.load();
    }

    void close() noexcept {
        /*
         * exchange() returns the previous value of fd_ and stores -1
         * in a single atomic operation. If two threads call close()
         * concurrently only the one that observes a non-negative value
         * actually performs ::shutdown and ::close, so the descriptor
         * is never closed twice.
         *
         * A concurrent reader in accept_connection() loads fd_ into a
         * local variable first. It therefore either sees the old value
         * (and gets EBADF from ::accept after shutdown has run) or the
         * new -1 (and throws before calling ::accept). Either way
         * there is no data race on the variable itself, and the accept
         * loop's catch handler observes running_ == false and exits.
         */
        const int fd = fd_.exchange(-1);

        if (fd >= 0) {
            ::shutdown(fd, SHUT_RDWR);
            ::close(fd);
        }
    }

   private:
    std::atomic<int> fd_{-1};
};

} // namespace caesar
