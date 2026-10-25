// Copyright (c) 2026 zaraiskais-byte. All rights reserved.
#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <caesar/p2p_frame.hpp>
#include <caesar/p2p_tcp.hpp>

namespace caesar {

class P2PConnection {
   public:
    P2PConnection() = default;

    explicit P2PConnection(P2PTcpSocket socket) : socket_(std::move(socket)) {
    }

    void connect_to(const std::string& address, std::uint16_t port) {
        socket_.connect_to(address, port);
    }

    void set_timeouts(int timeout_ms) {
        socket_.set_timeouts(timeout_ms);
    }

    bool valid() const noexcept {
        return socket_.valid();
    }

    void send_frame(const P2PFrame& frame) const {
        const auto encoded = frame.serialize_binary();

        std::lock_guard<std::mutex> lock(*send_mutex_);

        socket_.send_all(encoded.data(), encoded.size());
    }

    P2PFrame receive_frame() const {
        std::array<std::uint8_t, 4> length_bytes{};

        socket_.receive_all(length_bytes.data(), length_bytes.size());

        const std::uint32_t payload_size = static_cast<std::uint32_t>(length_bytes[0]) |
                                           (static_cast<std::uint32_t>(length_bytes[1]) << 8) |
                                           (static_cast<std::uint32_t>(length_bytes[2]) << 16) |
                                           (static_cast<std::uint32_t>(length_bytes[3]) << 24);

        if (payload_size > CZR_P2P_MAX_PAYLOAD)
            throw std::runtime_error("P2P payload too large");

        std::vector<std::uint8_t> encoded(5 + static_cast<std::size_t>(payload_size));

        std::memcpy(encoded.data(), length_bytes.data(), length_bytes.size());

        socket_.receive_all(encoded.data() + 4, encoded.size() - 4);

        return P2PFrame::deserialize_binary(encoded);
    }

    /*
     * Deadline-bounded variant of receive_frame().
     *
     * Semantically identical to receive_frame(), but the entire
     * read (length prefix + body) is bounded by an absolute
     * deadline rather than by per-recv() socket timeouts. Used by
     * the handshake path, where we must not let a slow-drip peer
     * hold the accept thread for an unbounded time.
     */
    P2PFrame receive_frame_with_deadline(
        std::chrono::steady_clock::time_point deadline) const {
        std::array<std::uint8_t, 4> length_bytes{};

        socket_.receive_all_until(deadline, length_bytes.data(), length_bytes.size());

        const std::uint32_t payload_size =
            static_cast<std::uint32_t>(length_bytes[0]) |
            (static_cast<std::uint32_t>(length_bytes[1]) << 8) |
            (static_cast<std::uint32_t>(length_bytes[2]) << 16) |
            (static_cast<std::uint32_t>(length_bytes[3]) << 24);

        if (payload_size > CZR_P2P_MAX_PAYLOAD)
            throw std::runtime_error("P2P payload too large");

        std::vector<std::uint8_t> encoded(5 + static_cast<std::size_t>(payload_size));

        std::memcpy(encoded.data(), length_bytes.data(), length_bytes.size());

        socket_.receive_all_until(deadline, encoded.data() + 4, encoded.size() - 4);

        return P2PFrame::deserialize_binary(encoded);
    }

   private:
    P2PTcpSocket socket_;
    std::shared_ptr<std::mutex> send_mutex_{std::make_shared<std::mutex>()};
};

} // namespace caesar
