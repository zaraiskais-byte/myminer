#pragma once

#include <mutex>
#include <string>
#include <vector>
#include <filesystem>
#include <sys/stat.h>
#include <fcntl.h>
#include <iostream>
#include <chrono>
#include <unistd.h>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <caesar/block.hpp>
#include <caesar/chain_validator.hpp>
#include <caesar/crypto.hpp>

namespace caesar {

namespace detail {

/*
 * Forces pending writes to a regular file to durable storage.
 *
 * std::ofstream::flush() only pushes bytes from the C++ stream buffer
 * into the OS page cache; it does not guarantee they reach disk.
 * After a power loss, a file that was "flushed" can still end up
 * truncated or missing.
 *
 * fsync() asks the kernel to flush the page cache for that file.
 * Combined with the atomic rename already present in save(), it gives
 * us two guarantees:
 *
 *   1. The bytes of the temporary file are on disk before the rename.
 *   2. The directory entry pointing at the new canonical file is
 *      durable after the rename.
 *
 * This is the standard write-ahead pattern used by Bitcoin Core and
 * most production databases.
 */
inline void fsync_file(const std::filesystem::path& p) {
    const int fd = ::open(p.c_str(), O_RDONLY);
    if (fd < 0) {
        throw std::runtime_error("fsync open failed for " + p.string() + ": " +
                                 std::strerror(errno));
    }

    if (::fsync(fd) != 0) {
        const int saved = errno;
        ::close(fd);
        throw std::runtime_error("fsync failed for " + p.string() + ": " + std::strerror(saved));
    }

    ::close(fd);
}

/*
 * Forces a directory entry update to disk.
 *
 * After rename(temp, canonical), the new name is visible to the
 * running system but the directory metadata may still sit in the page
 * cache. A crash before that metadata reaches disk would leave the
 * directory pointing at the old file. fsync() on the parent directory
 * closes that window.
 *
 * Some filesystems (notably f2fs on Android) return EINVAL when
 * fsync is called on a directory. That is not fatal: the rename is
 * still atomic, we just cannot force the directory entry to durable
 * storage. We swallow EINVAL and re-raise anything else, so tests on
 * Termux do not fail on f2fs while a genuine I/O error is still
 * surfaced.
 */
inline void fsync_parent_dir(const std::filesystem::path& p) {
    std::filesystem::path dir = p.parent_path();
    if (dir.empty())
        dir = ".";

    const int fd = ::open(dir.c_str(), O_RDONLY | O_DIRECTORY);
    if (fd < 0)
        return; // best-effort only

    if (::fsync(fd) != 0) {
        const int saved = errno;
        ::close(fd);
        if (saved == EINVAL) {
            // Filesystem does not support fsync on directories.
            return;
        }
        throw std::runtime_error("fsync dir failed for " + dir.string() + ": " +
                                 std::strerror(saved));
    }

    ::close(fd);
}

} // namespace detail

namespace detail {
struct LoadCache {
    std::mutex mu;
    std::string path;
    std::vector<Block> chain;
    unsigned long long ino = 0, size = 0, sec = 0, nsec = 0;
    bool ok = false;
};
inline LoadCache& load_cache() { static LoadCache c; return c; }
} // namespace detail

class BlockchainStorage {
   public:
    explicit BlockchainStorage(std::filesystem::path path) : path_(std::move(path)) {
    }

    const std::filesystem::path& path() const noexcept {
        return path_;
    }

    bool exists() const {
        return std::filesystem::exists(path_);
    }

    void save(const std::vector<Block>& chain) const {
        const auto diag_save_start = std::chrono::steady_clock::now();
        if (chain.empty())
            throw std::runtime_error("cannot save empty blockchain");

        const auto diag_validate_start = std::chrono::steady_clock::now();
        if (!validate_candidate_chain(chain))
            throw std::runtime_error("refusing to save invalid blockchain");
        std::cerr << "[storage:save] validate_ms="
                  << std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now() - diag_validate_start).count()
                  << std::endl;

        const auto temp = path_.string() + ".tmp";

        const auto diag_write_start = std::chrono::steady_clock::now();
        {
            std::ofstream out(temp, std::ios::binary | std::ios::trunc);

            if (!out)
                throw std::runtime_error("failed to create blockchain storage");

            write_u32(out, MAGIC);
            write_u32(out, FORMAT_VERSION);
            write_u64(out, static_cast<std::uint64_t>(chain.size()));

            for (const auto& block : chain)
                write_block(out, block);

            out.flush();

            if (!out)
                throw std::runtime_error("failed while writing blockchain storage");
        }

        std::cerr << "[storage:save] write_ms="
                  << std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now() - diag_write_start).count()
                  << std::endl;
        // The stream is now closed. Force its bytes to disk before the
        // rename makes the new file reachable under the canonical name.
        const auto diag_fsync_start = std::chrono::steady_clock::now();
        detail::fsync_file(temp);
        std::cerr << "[storage:save] fsync_file_ms="
                  << std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now() - diag_fsync_start).count()
                  << std::endl;

        // On the target platform (Linux/Termux), rename() replaces the
        // destination atomically. Never remove the existing canonical file
        // as a fallback: if replacement fails, the old chain must remain.
        std::error_code ec;
        std::filesystem::rename(temp, path_, ec);

        if (ec) {
            std::error_code cleanup_ec;
            std::filesystem::remove(temp, cleanup_ec);

            throw std::runtime_error("failed to atomically replace blockchain storage: " +
                                     ec.message());
        }

        // The rename is visible to this process, but the directory
        // entry may still be in the page cache. Force it to disk so a
        // crash does not leave the chain pointing at the old file.
        const auto diag_dir_start = std::chrono::steady_clock::now();
        detail::fsync_parent_dir(path_);
        {
            // chain was fully validated above and is now on disk: prime the load cache
            auto& lc2 = detail::load_cache();
            std::lock_guard<std::mutex> lk2(lc2.mu);
            struct stat st2;
            const std::string ps2 = std::filesystem::path(path_).string();
            if (::stat(ps2.c_str(), &st2) == 0) {
                lc2.chain = chain;
                lc2.path = ps2;
                lc2.ino = static_cast<unsigned long long>(st2.st_ino);
                lc2.size = static_cast<unsigned long long>(st2.st_size);
                lc2.sec = static_cast<unsigned long long>(st2.st_mtim.tv_sec);
                lc2.nsec = static_cast<unsigned long long>(st2.st_mtim.tv_nsec);
                lc2.ok = true;
            } else {
                lc2.ok = false;
            }
        }
        std::cerr << "[storage:save] fsync_dir_ms="
                  << std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now() - diag_dir_start).count()
                  << " total_ms="
                  << std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now() - diag_save_start).count()
                  << std::endl;
    }

    // Explicit full-chain replacement API.
    //
    // Validation and complete serialization happen before the final rename.
    // The existing canonical file is never removed as a fallback.
    void replace(const std::vector<Block>& chain) const {
        save(chain);
    }

    // czr-load-cache: reuse the fully validated chain while the storage file is unchanged
    std::vector<Block> load() const {
        auto& lc_ = detail::load_cache();
        std::mutex& cache_mu = lc_.mu;
        std::string& cache_path = lc_.path;
        std::vector<Block>& cache_chain = lc_.chain;
        unsigned long long& k_ino = lc_.ino;
        unsigned long long& k_size = lc_.size;
        unsigned long long& k_sec = lc_.sec;
        unsigned long long& k_nsec = lc_.nsec;
        bool& cache_ok = lc_.ok;
        const std::string p_str = std::filesystem::path(path_).string();
        auto read_key = [&](unsigned long long& ino, unsigned long long& size,
                            unsigned long long& sec, unsigned long long& nsec) -> bool {
            struct stat st;
            if (::stat(p_str.c_str(), &st) != 0) return false;
            ino = static_cast<unsigned long long>(st.st_ino);
            size = static_cast<unsigned long long>(st.st_size);
            sec = static_cast<unsigned long long>(st.st_mtim.tv_sec);
            nsec = static_cast<unsigned long long>(st.st_mtim.tv_nsec);
            return true;
        };
        std::lock_guard<std::mutex> cache_lock(cache_mu);
        unsigned long long a = 0, b = 0, c = 0, d = 0;
        const bool have_key = read_key(a, b, c, d);
        if (have_key && cache_ok && cache_path == p_str &&
            k_ino == a && k_size == b && k_sec == c && k_nsec == d) {
            return cache_chain;
        }
        std::vector<Block> fresh = load_uncached();
        unsigned long long a2 = 0, b2 = 0, c2 = 0, d2 = 0;
        if (have_key && read_key(a2, b2, c2, d2) && a == a2 && b == b2 && c == c2 && d == d2) {
            cache_chain = fresh;
            cache_path = p_str;
            k_ino = a; k_size = b; k_sec = c; k_nsec = d;
            cache_ok = true;
        } else {
            cache_ok = false;
        }
        return fresh;
    }

    std::vector<Block> load_uncached() const {
        const auto diag_load_start = std::chrono::steady_clock::now();
        std::ifstream in(path_, std::ios::binary);

        if (!in)
            throw std::runtime_error("failed to open blockchain storage");

        const auto magic = read_u32(in);
        const auto version = read_u32(in);
        const auto count = read_u64(in);

        if (magic != MAGIC)
            throw std::runtime_error("invalid blockchain storage magic");

        if (version != FORMAT_VERSION)
            throw std::runtime_error("unsupported blockchain storage version");

        if (count == 0 || count > MAX_BLOCKS)
            throw std::runtime_error("invalid blockchain block count");

        std::vector<Block> chain;
        chain.reserve(static_cast<std::size_t>(count));

        for (std::uint64_t i = 0; i < count; ++i)
            chain.push_back(read_block(in));

        if (in.peek() != std::ifstream::traits_type::eof())
            throw std::runtime_error("trailing bytes after blockchain storage");

        const auto diag_load_validate_start = std::chrono::steady_clock::now();
        if (!validate_candidate_chain(chain))
            throw std::runtime_error("stored blockchain failed canonical validation");
        std::cerr << "[storage:load] validate_ms="
                  << std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now() - diag_load_validate_start).count()
                  << " total_ms="
                  << std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now() - diag_load_start).count()
                  << " blocks=" << chain.size() << std::endl;

        return chain;
    }

    void append(const Block& block) const {
        std::vector<Block> chain;

        /*
         * validate_pow() is deterministic: calculate_pow_hash() is a
         * pure function of (header, nonce) with no PRNG, no cached
         * state, and no dependence on chain state. This preflight is
         * therefore a defensive assertion, not a real consistency
         * check. It exists to catch a hypothetical future regression
         * in which some non-determinism is introduced into the PoW
         * path. On a hot path it is expensive (65536-word memory
         * initialisation plus four mixing rounds); if append()
         * throughput ever becomes a concern, this is the first thing
         * to remove.
         */
        const bool pow_before_load = block.validate_pow();

        if (exists())
            chain = load();

        const bool pow_after_load = block.validate_pow();

        if (pow_before_load != pow_after_load)
            throw std::runtime_error(
                "PoW changed across storage load: before=" + std::to_string(pow_before_load) +
                " after=" + std::to_string(pow_after_load));

        if (chain.empty()) {
            if (!block.validate_basic())
                throw std::runtime_error("cannot store invalid first block");

            chain.push_back(block);
            save(chain);
            return;
        }

        const Block& previous = chain.back();

        if (block.header.height != previous.header.height + 1) {
            throw std::runtime_error("block height does not extend stored blockchain");
        }

        if (block.header.previous_hash != previous.hash()) {
            throw std::runtime_error("block previous hash does not match stored tip");
        }

        if (block.header.timestamp < previous.header.timestamp) {
            throw std::runtime_error("block timestamp is before stored tip");
        }

        const bool pow_after_checks = block.validate_pow();
        if (pow_after_load != pow_after_checks)
            throw std::runtime_error(
                "PoW changed after append checks: after_load=" + std::to_string(pow_after_load) +
                " after_checks=" + std::to_string(pow_after_checks));

        if (chain.size() < CZR_DIFFICULTY_WINDOW + 1) {
            const std::uint32_t expected_difficulty = expected_next_difficulty(chain);

            if (block.header.difficulty != expected_difficulty) {
                throw std::runtime_error("block difficulty does not match bootstrap rule");
            }

            if (!block.validate_basic()) {
                const bool v = block.header.version != 0;
                const bool pow = block.validate_pow();
                const bool coinbase =
                    validate_coinbase_position_and_reward(block.transactions, block.header.height);
                const bool merkle = block.validate_merkle_root();
                const bool witness = block.validate_witness_root();

                throw std::runtime_error(
                    "block basic validation failed: "
                    "version=" +
                    std::to_string(v) + " pow=" + std::to_string(pow) +
                    " coinbase=" + std::to_string(coinbase) + " merkle=" + std::to_string(merkle) +
                    " witness=" + std::to_string(witness) +
                    " height=" + std::to_string(block.header.height) +
                    " difficulty=" + std::to_string(block.header.difficulty));
            }
        } else {
            /*
             * Full consensus validation, matching the mining, relay,
             * and chain-replacement paths. Rebuilding the UTXO set
             * from the current chain is O(chain size), which matches
             * the cost already paid by every other
             * validate_block_consensus() caller in this codebase.
             *
             * This closes the previous gap where append() accepted
             * a block whose transactions did not spend existing
             * UTXOs, as long as the header and difficulty were
             * consistent with validate_against_chain().
             */
            const UTXOSet previous_utxos = rebuild_utxo_set(chain);

            if (!validate_block_consensus(block, chain, previous_utxos)) {
                throw std::runtime_error("block consensus validation failed");
            }
        }

        chain.push_back(block);
        save(chain);
    }

   private:
    static constexpr std::uint32_t MAGIC = 0x435A5231U;
    static constexpr std::uint32_t FORMAT_VERSION = 1;
    static constexpr std::uint64_t MAX_BLOCKS = 10000000ULL;
    static constexpr std::uint64_t MAX_BLOCK_BYTES = 16ULL * 1024ULL * 1024ULL;

    std::filesystem::path path_;

    static void write_u32(std::ofstream& out, std::uint32_t value) {
        for (unsigned i = 0; i < 4; ++i)
            out.put(static_cast<char>((value >> (i * 8)) & 0xffU));

        if (!out)
            throw std::runtime_error("failed writing uint32");
    }

    static void write_u64(std::ofstream& out, std::uint64_t value) {
        for (unsigned i = 0; i < 8; ++i)
            out.put(static_cast<char>((value >> (i * 8)) & 0xffULL));

        if (!out)
            throw std::runtime_error("failed writing uint64");
    }

    static std::uint32_t read_u32(std::ifstream& in) {
        std::uint32_t value = 0;

        for (unsigned i = 0; i < 4; ++i) {
            const int c = in.get();

            if (c == std::ifstream::traits_type::eof())
                throw std::runtime_error("unexpected end of blockchain storage");

            value |= static_cast<std::uint32_t>(static_cast<unsigned char>(c)) << (i * 8);
        }

        return value;
    }

    static std::uint64_t read_u64(std::ifstream& in) {
        std::uint64_t value = 0;

        for (unsigned i = 0; i < 8; ++i) {
            const int c = in.get();

            if (c == std::ifstream::traits_type::eof())
                throw std::runtime_error("unexpected end of blockchain storage");

            value |= static_cast<std::uint64_t>(static_cast<unsigned char>(c)) << (i * 8);
        }

        return value;
    }

    static void write_block(std::ofstream& out, const Block& block) {
        const auto data = block.serialize_full_binary();

        if (data.empty() || data.size() > MAX_BLOCK_BYTES) {
            throw std::runtime_error("block exceeds storage size limit");
        }

        if (data.size() > std::numeric_limits<std::uint32_t>::max())
            throw std::runtime_error("block is too large for storage format");

        const auto checksum = sha256(bytes_to_binary_string(data));

        write_u32(out, static_cast<std::uint32_t>(data.size()));

        out.write(reinterpret_cast<const char*>(data.data()),
                  static_cast<std::streamsize>(data.size()));

        for (const auto byte : checksum)
            out.put(static_cast<char>(byte));

        if (!out)
            throw std::runtime_error("failed writing blockchain block");
    }

    static Block read_block(std::ifstream& in) {
        const auto size = read_u32(in);

        if (size == 0 || size > MAX_BLOCK_BYTES)
            throw std::runtime_error("invalid stored block size");

        std::vector<std::uint8_t> data(size);

        in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));

        if (in.gcount() != static_cast<std::streamsize>(data.size())) {
            throw std::runtime_error("truncated stored block");
        }

        Hash256 stored_checksum{};

        in.read(reinterpret_cast<char*>(stored_checksum.data()),
                static_cast<std::streamsize>(stored_checksum.size()));

        if (in.gcount() != static_cast<std::streamsize>(stored_checksum.size())) {
            throw std::runtime_error("truncated stored block checksum");
        }

        const auto actual_checksum = sha256(bytes_to_binary_string(data));

        if (stored_checksum != actual_checksum)
            throw std::runtime_error("stored block checksum mismatch");

        return Block::deserialize_full(data);
    }
};

} // namespace caesar
