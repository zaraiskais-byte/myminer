#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

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
                std::move(connection)
            });

        return id;
    }

    bool contains(std::uint64_t id) const noexcept {
        return peers_.find(id) != peers_.end();
    }

    std::size_t size() const noexcept {
        return peers_.size();
    }

    const P2PPeerInfo& info(std::uint64_t id) const {
        const auto it = peers_.find(id);

        if (it == peers_.end())
            throw std::runtime_error("Unknown P2P peer");

        return it->second.info;
    }

    void remove_peer(std::uint64_t id) {
        peers_.erase(id);
    }

    void clear() noexcept {
        peers_.clear();
    }

    void send_to(
        std::uint64_t id,
        const P2PFrame& frame) {

        const auto it = peers_.find(id);

        if (it == peers_.end())
            throw std::runtime_error("Unknown P2P peer");

        it->second.connection.send_frame(frame);
    }

    void broadcast(const P2PFrame& frame) {

        for (auto& [id, peer] : peers_) {
            (void)id;
            peer.connection.send_frame(frame);
        }
    }

private:
    struct PeerEntry {
        P2PPeerInfo info;
        P2PConnection connection;
    };

    std::unordered_map<std::uint64_t, PeerEntry> peers_;
    std::uint64_t next_id_{1};
};

}
