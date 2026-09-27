/*
 * Storage durability test.
 *
 * BlockchainStorage::save() writes to a temporary file, then renames
 * it over the canonical path. That pattern is atomic with respect to
 * concurrent readers, but not necessarily durable across a power loss
 * unless the file contents and the directory entry are explicitly
 * synced to disk.
 *
 * This test does NOT simulate power loss (which cannot be done from
 * userspace). Instead it verifies the invariants that surround the
 * fsync calls:
 *
 *   1. save() leaves no .tmp file behind on success.
 *   2. Repeated save() calls keep the canonical file consistent.
 *   3. A failed save() (empty chain) does not modify the canonical
 *      file.
 *   4. A pre-existing .tmp file from a previous crash is overwritten
 *      cleanly by the next save() and does not affect load().
 *   5. Two consecutive storage instances see the same chain, proving
 *      the on-disk representation survives a fresh open.
 *
 * These invariants, combined with the fsync calls in save(), give the
 * expected crash-consistency behaviour on Linux / Termux.
 */

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#include <caesar/blockchain_storage.hpp>
#include <caesar/network_params.hpp>

using namespace caesar;

int main() {

    const auto base =
        std::filesystem::temp_directory_path() /
        "caesar_storage_durability_test";

    std::error_code ec;
    std::filesystem::remove_all(base, ec);
    std::filesystem::create_directories(base);

    const auto path = base / "blockchain.dat";
    const auto tmp_path =
        std::filesystem::path(path.string() + ".tmp");

    const Block genesis = build_canonical_genesis();

    // 1. Initial save.
    {
        BlockchainStorage storage(path);
        storage.save({genesis});
        assert(std::filesystem::exists(path));
        assert(!std::filesystem::exists(tmp_path));
    }
    std::cout << "[durability] initial save: OK\n";

    // 2. Repeated saves keep the file consistent.
    {
        BlockchainStorage storage(path);
        for (int i = 0; i < 5; ++i) {
            storage.save({genesis});
            const auto loaded = storage.load();
            assert(loaded.size() == 1);
            assert(loaded.front().hash() == genesis.hash());
            assert(!std::filesystem::exists(tmp_path));
        }
    }
    std::cout << "[durability] repeated saves: OK\n";

    // 3. Failed save does not damage the canonical file.
    {
        BlockchainStorage storage(path);
        const auto before = storage.load();

        bool threw = false;
        try {
            storage.save({});
        } catch (const std::exception&) {
            threw = true;
        }
        assert(threw);

        const auto after = storage.load();
        assert(after.size() == before.size());
        assert(after.front().hash() == before.front().hash());
    }
    std::cout << "[durability] failed save preserves canonical: OK\n";

    // 4. Leftover .tmp from a simulated crash does not break load().
    {
        {
            std::ofstream junk(
                tmp_path,
                std::ios::binary | std::ios::trunc);
            junk << "partial write from a killed process";
        }
        assert(std::filesystem::exists(tmp_path));

        BlockchainStorage storage(path);

        // load() reads the canonical file, not the temp file.
        const auto loaded = storage.load();
        assert(loaded.size() == 1);
        assert(loaded.front().hash() == genesis.hash());

        // save() overwrites the temp file with trunc.
        storage.save({genesis});
        assert(!std::filesystem::exists(tmp_path));

        const auto reloaded = storage.load();
        assert(reloaded.size() == 1);
        assert(reloaded.front().hash() == genesis.hash());
    }
    std::cout << "[durability] leftover tmp handled: OK\n";

    // 5. A fresh BlockchainStorage sees the same chain.
    {
        BlockchainStorage fresh(path);
        const auto loaded = fresh.load();
        assert(loaded.size() == 1);
        assert(loaded.front().hash() == genesis.hash());
    }
    std::cout << "[durability] fresh open sees same chain: OK\n";

    std::filesystem::remove_all(base, ec);

    std::cout << "CaesarStorageDurabilityTest: PASS\n";
    return 0;
}
