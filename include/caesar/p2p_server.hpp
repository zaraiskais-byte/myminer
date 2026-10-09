#pragma once

#include <arpa/inet.h>

#include <algorithm>
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
    static constexpr std::size_t MAX_TRACKED_CONNECTION_IPS = 1024;
    static constexpr std::chrono::seconds CONNECTION_ATTEMPT_WINDOW{10};

    P2PServer() = default;

    ~P2PServer() {
        stop();
    }

    P2PServer(const P2PServer&) = delete;
    P2PServer& operator=(const P2PServer&) = delete;

    void start(std::uint16_t port, const std::string& address = "0.0.0.0",
               std::uint32_t network_id = 1, std::uint64_t height = 0) {
        std::lock_guard<std::mutex> lifecycle_lock(
            lifecycle_mutex_);

        if (running_)
            throw std::runtime_error("P2P server already running");

        {
            std::lock_guard<std::mutex> lock(
                discovery_attempt_mutex_);
            discovery_attempts_.clear();
        }

        local_hello_.protocol_version = CZR_P2P_PROTOCOL_VERSION;

        local_hello_.network_id = network_id;

        local_hello_.height = height;

        local_hello_.timestamp = 0;

        local_hello_.user_agent = "Caesar-CZR";

        listener_.listen_on(port, address);

        listen_address_ = address;
        listen_port_ = port;

        running_ = true;

        try {
            accept_thread_ = std::thread([this]() { accept_loop(); });
        } catch (...) {
            running_ = false;
            listener_.close();
            throw;
        }
    }

    std::uint64_t connect_to_peer(const std::string& address,
                                  std::uint16_t port) {
        P2PHello local_hello;
        std::string listen_address;
        std::uint16_t listen_port = 0;

        {
            std::lock_guard<std::mutex> lifecycle_lock(
                lifecycle_mutex_);

            if (!running_)
                throw std::runtime_error("P2P server is not running");

            if (address.empty())
                throw std::runtime_error("P2P peer address is empty");

            if (port == 0)
                throw std::runtime_error("P2P peer port is invalid");

            if (peers_.size() >= P2PPeerManager::MAX_PEERS)
                throw std::runtime_error("P2P peer limit reached");

            /*
             * Snapshot lifecycle-owned connection state before
             * entering potentially blocking network operations.
             */
            local_hello = local_hello_;
            listen_address = listen_address_;
            listen_port = listen_port_;
        }

        /*
         * Outbound policy gate:
         * validate the endpoint and peer state before opening
         * a TCP socket. The transport itself is IPv4-only.
         */
        in_addr parsed_address{};
        if (::inet_pton(AF_INET, address.c_str(), &parsed_address) != 1)
            throw std::runtime_error("Invalid IPv4 address");

        if (port == listen_port) {
            const std::uint32_t host_address = ntohl(parsed_address.s_addr);
            const bool loopback =
                ((host_address >> 24) & 0xffU) == 127U;

            if (loopback ||
                (listen_address != "0.0.0.0" &&
                 address == listen_address)) {
                throw std::runtime_error("P2P outbound self-connection rejected");
            }
        }

        if (peers_.is_banned(address))
            throw std::runtime_error("P2P outbound peer is banned");

        const auto connected = peers_.peer_endpoints_snapshot();
        for (const auto& endpoint : connected) {
            if (endpoint.address == address && endpoint.port == port)
                throw std::runtime_error("P2P peer is already connected");
        }

        P2PConnection connection;

        connection.connect_to(address, port);

        if (!connection.valid())
            throw std::runtime_error("P2P outbound connection failed");

        connection.set_timeouts(5000);

        perform_hello_handshake(
            connection,
            local_hello,
            local_hello.network_id);

        /*
         * Re-enter the lifecycle gate only for the final state
         * transition. If stop() won the race, do not publish the
         * completed connection into the stopped server.
         */
        {
            std::lock_guard<std::mutex> lifecycle_lock(
                lifecycle_mutex_);

            if (!running_)
                throw std::runtime_error(
                    "P2P server stopped during outbound connection");

            if (peers_.size() >= P2PPeerManager::MAX_PEERS)
                throw std::runtime_error("P2P peer limit reached");

            if (peers_.is_banned(address))
                throw std::runtime_error("P2P outbound peer is banned");

            return peers_.add_peer(
                std::move(connection),
                address,
                port);
        }
    }

    void stop() noexcept {
        std::lock_guard<std::mutex> lifecycle_lock(
            lifecycle_mutex_);

        if (!running_)
            return;

        running_ = false;

        listener_.close();

        if (accept_thread_.joinable())
            accept_thread_.join();

        peers_.clear();

        {
            std::lock_guard<std::mutex> lock(
                discovery_attempt_mutex_);
            discovery_attempts_.clear();
        }
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

    const P2PPeerManager& peers() const noexcept {
        return peers_;
    }

    /*
     * Promote a bounded number of validated discovery candidates
     * into outbound peers.
     *
     * This method is intentionally caller-driven. It does not
     * create a background thread and is never invoked directly
     * from the network receive path.
     */
    std::size_t connect_to_discovered_peers(
        std::size_t max_attempts = 4) {

        if (!running_ || max_attempts == 0)
            return 0;

        constexpr std::size_t MAX_DISCOVERY_ATTEMPTS_PER_RUN = 4;

        const std::size_t attempts_allowed =
            std::min(max_attempts, MAX_DISCOVERY_ATTEMPTS_PER_RUN);

        if (peers_.size() >= P2PPeerManager::MAX_PEERS)
            return 0;

        const auto candidates =
            peers_.known_peer_endpoints_snapshot();

        std::size_t attempts = 0;
        std::size_t connected = 0;

        for (const auto& endpoint : candidates) {
            if (attempts >= attempts_allowed)
                break;

            if (peers_.size() >= P2PPeerManager::MAX_PEERS)
                break;

            const std::string key =
                endpoint.address + ":" +
                std::to_string(endpoint.port);

            const auto now = std::chrono::steady_clock::now();

            {
                std::lock_guard<std::mutex> lock(
                    discovery_attempt_mutex_);

                /*
                 * Expire retry state so failed discovery
                 * candidates cannot accumulate forever.
                 */
                for (auto it = discovery_attempts_.begin();
                     it != discovery_attempts_.end();) {
                    if (now - it->second >=
                        DISCOVERY_RETRY_COOLDOWN) {
                        it = discovery_attempts_.erase(it);
                    } else {
                        ++it;
                    }
                }

                const auto it =
                    discovery_attempts_.find(key);

                if (it != discovery_attempts_.end() &&
                    now - it->second <
                        DISCOVERY_RETRY_COOLDOWN) {
                    continue;
                }

                if (discovery_attempts_.size() >=
                    MAX_DISCOVERY_RETRY_STATE) {
                    continue;
                }

                discovery_attempts_[key] = now;
            }

            ++attempts;

            try {
                connect_to_peer(endpoint.address, endpoint.port);

                peers_.forget_peer_endpoint(
                    endpoint.address,
                    endpoint.port);

                {
                    std::lock_guard<std::mutex> lock(
                        discovery_attempt_mutex_);
                    discovery_attempts_.erase(key);
                }

                ++connected;
            } catch (const std::exception&) {
                /*
                 * Keep the candidate for a later discovery
                 * cycle, but prevent immediate retry storms.
                 */
            }
        }

        return connected;
    }

   private:
    bool allow_connection_attempt(const std::string& address) {
        const auto now = std::chrono::steady_clock::now();

        std::lock_guard<std::mutex> lock(attempt_mutex_);

        for (auto it = connection_attempts_.begin(); it != connection_attempts_.end();) {
            if (now - it->second.window_start >= CONNECTION_ATTEMPT_WINDOW) {
                it = connection_attempts_.erase(it);
            } else {
                ++it;
            }
        }

        auto it = connection_attempts_.find(address);

        if (it == connection_attempts_.end()) {
            if (connection_attempts_.size() >= MAX_TRACKED_CONNECTION_IPS) {
                return false;
            }

            connection_attempts_.emplace(address, ConnectionAttemptState{now, 1});

            return true;
        }

        auto& state = it->second;

        if (state.attempts >= MAX_CONNECTION_ATTEMPTS_PER_IP) {
            return false;
        }

        ++state.attempts;
        return true;
    }

    void accept_loop() {
        while (running_) {
            try {
                P2PTcpSocket socket = listener_.accept_connection();

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

                const std::string peer_address = socket.peer_address();

                if (!allow_connection_attempt(peer_address)) {
                    socket.close();
                    continue;
                }

                const std::uint16_t peer_port = socket.peer_port();

                P2PConnection connection(std::move(socket));

                perform_hello_handshake(connection, local_hello_, local_hello_.network_id);

                if (!running_) {
                    break;
                }

                peers_.add_peer(std::move(connection), peer_address, peer_port);

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

    std::string listen_address_;
    std::uint16_t listen_port_{0};

    mutable std::mutex attempt_mutex_;
    std::unordered_map<std::string, ConnectionAttemptState> connection_attempts_;

    static constexpr auto DISCOVERY_RETRY_COOLDOWN =
        std::chrono::seconds(30);

    static constexpr std::size_t MAX_DISCOVERY_RETRY_STATE =
        256;

    mutable std::mutex discovery_attempt_mutex_;
    std::unordered_map<
        std::string,
        std::chrono::steady_clock::time_point>
        discovery_attempts_;

    P2PHello local_hello_;

    mutable std::mutex lifecycle_mutex_;
    std::atomic<bool> running_{false};
    std::thread accept_thread_;
};

} // namespace caesar
