/*
 * Regression test for the P2PRelay lifecycle race.
 *
 * On the buggy code, P2PRelay::on_peer_added() checked running_ BEFORE
 * taking threads_mutex_, while stop() set running_ = false and swapped
 * threads_ under that lock. The two sequences could interleave so that
 * a worker thread was appended to threads_ AFTER stop() had already
 * swapped the vector, which means the worker was never joined.
 *
 * This test spawns a writer thread calling on_peer_added() repeatedly
 * and a stopper thread calling stop(), then asserts that after stop()
 * returns the threads_ vector is empty. With the race present, some
 * iterations leave an unjoined worker in threads_; with the fix, every
 * iteration ends clean.
 */

#include <atomic>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

// Testing-only access to P2PRelay private members.
#define private public
#include <caesar/p2p_relay.hpp>
#undef private

#include <caesar/blockchain_storage.hpp>
#include <caesar/p2p_server.hpp>

using namespace caesar;

int main() {

    const auto tmp =
        std::filesystem::temp_directory_path() /
        "caesar_p2p_lifecycle_race_test.dat";

    std::error_code ec;
    std::filesystem::remove(tmp, ec);

    P2PServer server;
    BlockchainStorage storage(tmp);
    auto storage_mutex = std::make_shared<std::mutex>();

    constexpr int iterations = 200;
    int races_detected = 0;

    for (int it = 0; it < iterations; ++it) {

        P2PRelay relay(server, storage, storage_mutex);
        relay.start();

        std::atomic<bool> go{false};

        std::thread writer([&]() {
            while (!go.load(std::memory_order_acquire)) {}
            for (int i = 0; i < 100; ++i)
                relay.on_peer_added(
                    static_cast<std::uint64_t>(i));
        });

        std::thread stopper([&]() {
            while (!go.load(std::memory_order_acquire)) {}
            relay.stop();
        });

        go.store(true, std::memory_order_release);

        writer.join();
        stopper.join();

        std::size_t leftover = 0;
        {
            std::lock_guard<std::mutex> lock(relay.threads_mutex_);
            leftover = relay.threads_.size();
        }

        if (leftover != 0) {
            ++races_detected;
            std::cout << "[iter " << it << "] RACE: "
                      << leftover << " worker(s) left in threads_"
                      << " after stop()\n";

            std::vector<std::thread> drain;
            {
                std::lock_guard<std::mutex> lock(relay.threads_mutex_);
                drain.swap(relay.threads_);
            }
            for (auto& t : drain)
                if (t.joinable()) t.join();
        }
    }

    std::filesystem::remove(tmp, ec);

    std::cout << "[p2p-lifecycle] iterations=" << iterations
              << " races_detected=" << races_detected << "\n";

    if (races_detected > 0) {
        std::cerr << "REGRESSION: P2PRelay::stop() left unjoined "
                     "workers behind\n";
        return 1;
    }

    std::cout << "CaesarP2PLifecycleRaceTest: PASS\n";
    return 0;
}
