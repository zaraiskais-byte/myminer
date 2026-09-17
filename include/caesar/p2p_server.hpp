#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#include <caesar/p2p_handshake.hpp>
#include <caesar/p2p_peer_manager.hpp>
#include <caesar/p2p_tcp.hpp>

namespace caesar {

class P2PServer {
public:
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
                    "unknown",
                    0);

            } catch (...) {

                if (!running_)
                    break;
            }
        }
    }

    P2PTcpSocket listener_;
    P2PPeerManager peers_;

    P2PHello local_hello_;

    std::atomic<bool> running_{false};
    std::thread accept_thread_;
};

}
