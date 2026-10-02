#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
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
    int score{100};
};

class P2PPeerManager {
   public:
    static constexpr std::size_t MAX_PEERS = 64;

    /*
     * Peer scoring.
     *
     * Each connected peer starts with PEER_SCORE_INITIAL points.
     * Successfully handled frames reward DEFAULT_REWARD points (capped
     * at PEER_SCORE_MAX). Any protocol error, malformed frame, or
     * handler exception penalizes DEFAULT_PENALTY points.
     *
     * When a peer's score reaches BAN_THRESHOLD (zero), its address is
     * added to a ban list for DEFAULT_BAN_DURATION. Subsequent
     * add_peer() calls with that address throw, so a misbehaving
     * peer cannot immediately reconnect.
     *
     * Banning is per-address, not per-connection-id, because ids are
     * assigned fresh on every connection.
     */
    static constexpr int PEER_SCORE_INITIAL = 100;
    static constexpr int PEER_SCORE_MAX = 200;
    static constexpr int PEER_SCORE_MIN = 0;
    static constexpr int BAN_THRESHOLD = 0;
    static constexpr int DEFAULT_PENALTY = 10;
    static constexpr int DEFAULT_REWARD = 1;

    using BanDuration = std::chrono::seconds;
    static constexpr BanDuration DEFAULT_BAN_DURATION = std::chrono::hours(24);

    P2PPeerManager() = default;

    std::uint64_t add_peer(P2PConnection connection, const std::string& address,
                           std::uint16_t port) {
        if (!connection.valid())
            throw std::runtime_error("Cannot add invalid P2P peer");

        std::uint64_t id = 0;
        std::function<void(std::uint64_t)> callback;

        {
            std::lock_guard<std::mutex> lock(mutex_);

            if (peers_.size() >= MAX_PEERS)
                throw std::runtime_error("P2P peer limit reached");

            if (is_banned_locked(address)) {
                throw std::runtime_error("Cannot add banned P2P peer");
            }

            id = next_id_++;

            peers_.emplace(id, PeerEntry{P2PPeerInfo{id, address, port, true, PEER_SCORE_INITIAL},
                                         std::make_shared<P2PConnection>(std::move(connection))});

            callback = peer_added_callback_;
        }

        if (callback)
            callback(id);

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

    P2PPeerInfo info(std::uint64_t id) const {
        std::lock_guard<std::mutex> lock(mutex_);
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

    void send_to(std::uint64_t id, const P2PFrame& frame) {
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

    void set_peer_added_callback(std::function<void(std::uint64_t)> cb) {
        std::lock_guard<std::mutex> lock(mutex_);
        peer_added_callback_ = std::move(cb);
    }

    std::shared_ptr<P2PConnection> connection(std::uint64_t id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = peers_.find(id);

        if (it == peers_.end())
            return nullptr;

        return it->second.connection;
    }

    void broadcast_except(std::uint64_t excluded_id, const P2PFrame& frame) {
        std::vector<std::shared_ptr<P2PConnection>> connections;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            connections.reserve(peers_.size());
            for (const auto& kv : peers_) {
                if (kv.first == excluded_id) continue;
                connections.push_back(kv.second.connection);
            }
        }
        for (const auto& conn : connections) {
            try { conn->send_frame(frame); } catch (...) {}
        }
    }

    std::vector<std::uint64_t> list_ids() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::uint64_t> ids;
        ids.reserve(peers_.size());
        for (const auto& kv : peers_) ids.push_back(kv.first);
        return ids;
    }

    /*
     * Returns the current score of a connected peer, or 0 if the id
     * is unknown.
     */
    int score(std::uint64_t id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = peers_.find(id);
        if (it == peers_.end())
            return 0;
        return it->second.info.score;
    }

    /*
     * Decrements a peer's score. If the score reaches BAN_THRESHOLD,
     * the peer is banned by address and removed from the active set.
     */
    void penalize(std::uint64_t id, int amount) {
        std::string address_to_ban;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            const auto it = peers_.find(id);
            if (it == peers_.end())
                return;

            it->second.info.score -= amount;

            if (it->second.info.score <= BAN_THRESHOLD) {
                it->second.info.score = PEER_SCORE_MIN;
                address_to_ban = it->second.info.address;
            }
        }

        if (!address_to_ban.empty()) {
            ban(address_to_ban);
            remove_peer(id);
        }
    }

    /*
     * Increments a peer's score, capped at PEER_SCORE_MAX.
     */
    void reward(std::uint64_t id, int amount) {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = peers_.find(id);
        if (it == peers_.end())
            return;

        it->second.info.score += amount;
        if (it->second.info.score > PEER_SCORE_MAX)
            it->second.info.score = PEER_SCORE_MAX;
    }

    /*
     * Returns true if the address is currently banned.
     */
    bool is_banned(const std::string& address) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return is_banned_locked(address);
    }

    /*
     * Adds the address to the ban list for DEFAULT_BAN_DURATION.
     * Idempotent: re-banning refreshes the ban window.
     */
    void ban(const std::string& address) {
        std::lock_guard<std::mutex> lock(mutex_);
        banned_addresses_[address] = std::chrono::steady_clock::now() + DEFAULT_BAN_DURATION;
    }

    /*
     * Removes the address from the ban list.
     */
    void unban(const std::string& address) {
        std::lock_guard<std::mutex> lock(mutex_);
        banned_addresses_.erase(address);
    }

   private:
    struct PeerEntry {
        P2PPeerInfo info;
        std::shared_ptr<P2PConnection> connection;
    };

    /*
     * Caller must hold mutex_. Removes expired bans in the process.
     * Returns true if the address is still inside its ban window.
     */
    bool is_banned_locked(const std::string& address) const {
        const auto it = banned_addresses_.find(address);
        if (it == banned_addresses_.end())
            return false;

        if (it->second <= std::chrono::steady_clock::now()) {
            banned_addresses_.erase(it);
            return false;
        }

        return true;
    }

    std::unordered_map<std::uint64_t, PeerEntry> peers_;
    /*
     * mutable so that is_banned_locked() -- which is const because it
     * is called from the const method is_banned() -- can lazily drop
     * expired entries. The mutex_ member is mutable for the same
     * reason.
     */
    mutable std::unordered_map<std::string, std::chrono::steady_clock::time_point>
        banned_addresses_;
    mutable std::mutex mutex_;
    std::uint64_t next_id_{1};
    std::function<void(std::uint64_t)> peer_added_callback_;
};

} // namespace caesar
