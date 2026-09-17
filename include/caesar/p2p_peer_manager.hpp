#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <caesar/p2p_connection.hpp>

namespace caesar {

struct P2PPeerInfo {
    std::uint64_t id{0};
    std::string address;
    std::uint16_t port{0};
    bool connected{false};
};

class P2PPeerManager {
public:
    P2PPeerManager() = default;

    std::uint64_t add_peer(
        P2PConnection connection,
        const std::string& address,
        std::uint16_t port) {

        if (!connection.valid())
            throw std::runtime_error("Cannot add invalid P2P peer");

        std::lock_guard<std::mutex> lock(mutex_);
        const std::uint64_t id = next_id_++;

        peers_.emplace(
            id,
            PeerEntry{
                P2PPeerInfo{
                    id,
                    address,
                    port,
                    true
                },
                std::make_shared<P2PConnection>(std::move(connection))
            });

        return id;
    }

    bool contains(std::uint64_t id) const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return peers_.find(id) != peers_.end();
    }

    std::size_t size() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return peers_.size();
    }

    const P2PPeerInfo& info(std::uint64_t id) const {
        const auto it = peers_.find(id);

        if (it == peers_.end())
            throw std::runtime_error("Unknown P2P peer");

        return it->second.info;
    }

    void remove_peer(std::uint64_t id) {
        std::lock_guard<std::mutex> lock(mutex_);
        peers_.erase(id);
    }

    void clear() noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        peers_.clear();
    }

    void send_to(
        std::uint64_t id,
        const P2PFrame& frame) {

        std::shared_ptr<P2PConnection> connection;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            const auto it = peers_.find(id);

            if (it == peers_.end())
                throw std::runtime_error("Unknown P2P peer");

            connection = it->second.connection;
        }

        connection->send_frame(frame);
    }

    void broadcast(const P2PFrame& frame) {

        std::vector<std::shared_ptr<P2PConnection>> connections;

        {
            std::lock_guard<std::mutex> lock(mutex_);

            connections.reserve(peers_.size());

            for (const auto& [id, peer] : peers_) {
                (void)id;
                connections.push_back(peer.connection);
            }
        }

        for (const auto& connection : connections)
            connection->send_frame(frame);
    }

private:
    struct PeerEntry {
        P2PPeerInfo info;
        std::shared_ptr<P2PConnection> connection;
    };

    std::unordered_map<std::uint64_t, PeerEntry> peers_;
    mutable std::mutex mutex_;
    std::uint64_t next_id_{1};
};

}
