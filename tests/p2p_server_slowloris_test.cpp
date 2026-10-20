// Copyright (c) 2026 zaraiskais-byte. All rights reserved.
/*
 * C7-2 regression test: slowloris against the handshake path.
 *
 * Threat model
 * ------------
 * P2PServer runs a single accept thread. Before eadd460, that
 * thread called receive_all() with SO_RCVTIMEO set per-recv().
 * A peer that opened a TCP connection and then dribbled one
 * byte every few seconds could hold the accept thread in
 * receive_all() for an unbounded time: each individual recv()
 * returned in under the 5-second SO_RCVTIMEO, so the socket
 * timeout never fired, and the accept thread never moved on
 * to the next connection. That is a slowloris-style DoS
 * against the whole P2P listener.
 *
 * Fix under test
 * --------------
 * receive_all_until(deadline, ...) enforces an absolute
 * deadline across the whole logical read via poll(). The
 * handshake path (perform_hello_handshake) uses it with a
 * 5-second budget.
 *
 * What this test does
 * -------------------
 *   1. Start a real P2PServer on a loopback port.
 *   2. Open a raw TCP socket (NOT P2PConnection, because we
 *      want byte-level control).
 *   3. Send exactly one byte of the 4-byte length prefix that
 *      precedes every P2P frame.
 *   4. Stop sending. The server blocks waiting for the rest.
 *   5. Assert the server closes the connection within a
 *      wall-clock budget that is close to the handshake
 *      deadline (5s), not hundreds of seconds.
 *
 * If someone later removes the deadline and reverts to the
 * per-recv() SO_RCVTIMEO behaviour, this test will fail with
 * a "server did not close" message instead of silently
 * regressing.
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <thread>

#include <caesar/p2p_server.hpp>

namespace {

/*
 * Wait until the server closes the raw client socket, or until
 * the hard deadline expires.
 *
 * Returns true if the socket is observed closed (recv() == 0
 * or a socket error). Returns false if the deadline expires
 * without any close.
 *
 * The server sends its own Hello frame right after accept; we
 * must consume those bytes and keep waiting, otherwise poll()
 * would keep firing on readable data and we would never
 * observe the close.
 */
bool wait_for_server_close(
    int fd,
    std::chrono::steady_clock::time_point hard_deadline) {

    while (std::chrono::steady_clock::now() < hard_deadline) {
        pollfd pfd{};
        pfd.fd = fd;
        pfd.events = POLLIN;

        const int poll_result = ::poll(&pfd, 1, 500);

        if (poll_result < 0)
            return true;   /* error: treat as closed */

        if (poll_result == 0)
            continue;      /* no events; keep waiting */

        std::uint8_t buf[256];
        const ssize_t r = ::recv(fd, buf, sizeof(buf), 0);

        if (r == 0)
            return true;   /* EOF: server closed */
        if (r < 0)
            return true;   /* ECONNRESET etc. */

        /* Server sent bytes (its Hello). Consume and keep
         * waiting for the close. */
    }

    return false;
}

}  /* namespace */

int main() {
    using namespace caesar;
    using namespace std::chrono;

    constexpr std::uint16_t port = 39428;
    constexpr std::uint32_t network_id = 1;

    P2PServer server;
    server.start(port, "127.0.0.1", network_id, 100);

    if (!server.running()) {
        std::cerr << "[FAIL] server did not start\n";
        return EXIT_FAILURE;
    }

    /* Let the accept loop reach its blocking ::accept() call
     * before we connect. */
    std::this_thread::sleep_for(milliseconds(200));

    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        std::cerr << "[FAIL] raw socket() failed\n";
        server.stop();
        return EXIT_FAILURE;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) != 1) {
        std::cerr << "[FAIL] inet_pton failed\n";
        ::close(fd);
        server.stop();
        return EXIT_FAILURE;
    }

    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr),
                  sizeof(addr)) != 0) {
        std::cerr << "[FAIL] connect() failed\n";
        ::close(fd);
        server.stop();
        return EXIT_FAILURE;
    }

    /* Send exactly ONE byte of the 4-byte big-endian length
     * prefix that precedes every P2P frame. The server will
     * block in receive_all_until() waiting for three more
     * bytes, which we never send. */
    const std::uint8_t partial_prefix[1] = {0x00};
    const ssize_t sent = ::send(fd, partial_prefix, 1, 0);
    if (sent != 1) {
        std::cerr << "[FAIL] send() returned " << sent << "\n";
        ::close(fd);
        server.stop();
        return EXIT_FAILURE;
    }

    const auto t0 = steady_clock::now();

    /* perform_hello_handshake() uses a 5-second budget. Give
     * the server 10 seconds of wall-clock to close the socket
     * on a slow mobile CPU. */
    const auto hard_deadline = t0 + seconds(10);

    const bool closed = wait_for_server_close(fd, hard_deadline);
    const auto elapsed_ms =
        duration_cast<milliseconds>(steady_clock::now() - t0).count();

    ::close(fd);

    std::cout << "server closed the slowloris connection after "
              << elapsed_ms << " ms\n";

    bool ok = true;
    if (!closed) {
        std::cerr << "[FAIL] server did not close the connection "
                     "within the 10-second wall-clock budget\n";
        ok = false;
    } else if (elapsed_ms < 1000) {
        std::cerr << "[FAIL] server closed after only " << elapsed_ms
                  << " ms; the handshake deadline may have been "
                     "removed or bypassed\n";
        ok = false;
    } else if (elapsed_ms > 8000) {
        std::cerr << "[FAIL] server took " << elapsed_ms
                  << " ms to close; expected <= 8000 ms\n";
        ok = false;
    } else {
        std::cout << "[PASS] slowloris bounded by handshake deadline\n";
    }

    server.stop();
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
