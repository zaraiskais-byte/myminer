#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>

#include <caesar/p2p_handshake.hpp>
#include <caesar/p2p_peer_manager.hpp>
#include <caesar/p2p_tcp.hpp>

namespace caesar {

class P2PServer {
public:
    static constexpr std::size_t MAX_CONNECTION_ATTEMPTS_PER_IP = 8;
    static constexpr std::chrono::seconds CONNECTION_ATTEMPT_WINDOW{10};

    P2PServer() = default;

    ~P2PServer() {
        stop();
    }

    P2PServer(const P2PServer&) = delete;
    P2PServer& operator=(const P2PServer&) = delete;

    void start(
        std::uint16_t port,
        const std::string& address = "0.0.0.0",
        std::uint32_t network_id = 1,
        std::uint64_t height = 0) {

        if (running_)
            throw std::runtime_error(
                "P2P server already running");

        local_hello_.protocol_version =
            CZR_P2P_PROTOCOL_VERSION;

        local_hello_.network_id =
            network_id;

        local_hello_.height =
            height;

        local_hello_.timestamp = 0;

        local_hello_.user_agent =
            "Caesar-CZR";

        listener_.listen_on(port, address);

        running_ = true;

        accept_thread_ = std::thread(
            [this]() {
                accept_loop();
            });
    }

    void stop() noexcept {

        if (!running_)
            return;

        running_ = false;

        listener_.close();

        if (accept_thread_.joinable())
            accept_thread_.join();

        peers_.clear();
    }

    bool running() const noexcept {
        return running_;
    }

    std::size_t peer_count() const noexcept {
        return peers_.size();
    }

    P2PPeerManager& peers() noexcept {
        return peers_;
    }

private:
    bool allow_connection_attempt(
        const std::string& address) {

        const auto now =
            std::chrono::steady_clock::now();

        std::lock_guard<std::mutex> lock(
            attempt_mutex_);

        auto& state =
            connection_attempts_[address];

        if (state.attempts == 0 ||
            now - state.window_start >=
                CONNECTION_ATTEMPT_WINDOW) {

            state.window_start = now;
            state.attempts = 1;
            return true;
        }

        if (state.attempts >=
            MAX_CONNECTION_ATTEMPTS_PER_IP) {
            return false;
        }

        ++state.attempts;
        return true;
    }

    void accept_loop() {

        while (running_) {

            try {

                P2PTcpSocket socket =
                    listener_.accept_connection();

                if (!running_) {
                    socket.close();
                    break;
                }

                socket.set_timeouts(5000);

                // Security gate: never spend handshake resources when
                // the peer table is already at its hard capacity.
                if (peers_.size() >= P2PPeerManager::MAX_PEERS) {
                    socket.close();
                    continue;
                }

                const std::string peer_address =
                    socket.peer_address();

                if (!allow_connection_attempt(
                        peer_address)) {
                    socket.close();
                    continue;
                }

                const std::uint16_t peer_port =
                    socket.peer_port();

                P2PConnection connection(
                    std::move(socket));

                perform_hello_handshake(
                    connection,
                    local_hello_,
                    local_hello_.network_id);

                if (!running_) {
                    break;
                }

                peers_.add_peer(
                    std::move(connection),
                    peer_address,
                    peer_port);

            } catch (...) {

                if (!running_)
                    break;
            }
        }
    }

    struct ConnectionAttemptState {
        std::chrono::steady_clock::time_point window_start{};
        std::size_t attempts{0};
    };

    P2PTcpSocket listener_;
    P2PPeerManager peers_;

    mutable std::mutex attempt_mutex_;
    std::unordered_map<std::string, ConnectionAttemptState>
        connection_attempts_;

    P2PHello local_hello_;

    std::atomic<bool> running_{false};
    std::thread accept_thread_;
};

}
