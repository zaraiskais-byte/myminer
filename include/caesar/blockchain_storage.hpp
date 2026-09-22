#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <caesar/block.hpp>
#include <caesar/crypto.hpp>

namespace caesar {

class BlockchainStorage {
public:
    explicit BlockchainStorage(std::filesystem::path path)
        : path_(std::move(path)) {}

    const std::filesystem::path& path() const noexcept {
        return path_;
    }

    bool exists() const {
        return std::filesystem::exists(path_);
    }

    void save(const std::vector<Block>& chain) const {
        if (chain.empty())
            throw std::runtime_error("cannot save empty blockchain");

        if (!validate_block_chain(chain))
            throw std::runtime_error("refusing to save invalid blockchain");

        const auto temp = path_.string() + ".tmp";

        {
            std::ofstream out(
                temp,
                std::ios::binary | std::ios::trunc);

            if (!out)
                throw std::runtime_error(
                    "failed to create blockchain storage");

            write_u32(out, MAGIC);
            write_u32(out, FORMAT_VERSION);
            write_u64(out, static_cast<std::uint64_t>(chain.size()));

            for (const auto& block : chain)
                write_block(out, block);

            out.flush();

            if (!out)
                throw std::runtime_error(
                    "failed while writing blockchain storage");
        }

        std::error_code ec;
        std::filesystem::rename(temp, path_, ec);

        if (ec) {
            std::filesystem::remove(path_, ec);
            ec.clear();
            std::filesystem::rename(temp, path_, ec);
        }

        if (ec) {
            std::filesystem::remove(temp, ec);
            throw std::runtime_error(
                "failed to replace blockchain storage");
        }
    }

    std::vector<Block> load() const {
        std::ifstream in(
            path_,
            std::ios::binary);

        if (!in)
            throw std::runtime_error(
                "failed to open blockchain storage");

        const auto magic = read_u32(in);
        const auto version = read_u32(in);
        const auto count = read_u64(in);

        if (magic != MAGIC)
            throw std::runtime_error(
                "invalid blockchain storage magic");

        if (version != FORMAT_VERSION)
            throw std::runtime_error(
                "unsupported blockchain storage version");

        if (count == 0 || count > MAX_BLOCKS)
            throw std::runtime_error(
                "invalid blockchain block count");

        std::vector<Block> chain;
        chain.reserve(static_cast<std::size_t>(count));

        for (std::uint64_t i = 0; i < count; ++i)
            chain.push_back(read_block(in));

        if (in.peek() != std::ifstream::traits_type::eof())
            throw std::runtime_error(
                "trailing bytes after blockchain storage");

        if (!validate_block_chain(chain))
            throw std::runtime_error(
                "stored blockchain failed validation");

        return chain;
    }

    bool replace_chain(
        const std::vector<Block>& candidate) const {

        if (candidate.empty())
            throw std::runtime_error(
                "cannot replace blockchain with empty chain");

        if (!validate_block_chain(candidate))
            throw std::runtime_error(
                "refusing to replace blockchain with invalid chain");

        std::vector<Block> current;

        if (exists())
            current = load();

        if (current.empty()) {
            save(candidate);
            return true;
        }

        const ChainWork candidate_work =
            calculate_chain_work(candidate);

        const ChainWork current_work =
            calculate_chain_work(current);

        // Fork choice is strictly greater cumulative PoW work.
        // Equal-work and lower-work candidates do not reorg the node.
        if (candidate_work <= current_work)
            return false;

        save(candidate);
        return true;
    }

    void append(const Block& block) const {
        std::vector<Block> chain;

        const bool pow_before_load = block.validate_pow();

        if (exists())
            chain = load();

        const bool pow_after_load = block.validate_pow();

        if (pow_before_load != pow_after_load)
            throw std::runtime_error(
                "PoW changed across storage load: before=" +
                std::to_string(pow_before_load) +
                " after=" +
                std::to_string(pow_after_load));

        if (chain.empty()) {
            if (!block.validate_basic())
                throw std::runtime_error(
                    "cannot store invalid first block");

            chain.push_back(block);
            save(chain);
            return;
        }

        const Block& previous = chain.back();

        if (block.header.height !=
            previous.header.height + 1) {
            throw std::runtime_error(
                "block height does not extend stored blockchain");
        }

        if (block.header.previous_hash !=
            previous.hash()) {
            throw std::runtime_error(
                "block previous hash does not match stored tip");
        }

        if (block.header.timestamp <
            previous.header.timestamp) {
            throw std::runtime_error(
                "block timestamp is before stored tip");
        }

        const bool pow_after_checks = block.validate_pow();
        if (pow_after_load != pow_after_checks)
            throw std::runtime_error(
                "PoW changed after append checks: after_load=" +
                std::to_string(pow_after_load) +
                " after_checks=" +
                std::to_string(pow_after_checks));

        if (chain.size() < CZR_DIFFICULTY_WINDOW + 1) {
            const std::uint32_t expected_difficulty =
                (previous.header.height == 0 &&
                 previous.header.difficulty == 0)
                    ? CZR_INITIAL_MINING_DIFFICULTY
                    : previous.header.difficulty;

            if (block.header.difficulty !=
                expected_difficulty) {
                throw std::runtime_error(
                    "block difficulty does not match bootstrap rule");
            }

            if (!block.validate_basic()) {
                const bool v = block.header.version != 0;
                const bool pow = block.validate_pow();
                const bool coinbase =
                    validate_coinbase_position_and_reward(
                        block.transactions,
                        block.header.height);
                const bool merkle = block.validate_merkle_root();
                const bool witness = block.validate_witness_root();

                throw std::runtime_error(
                    "block basic validation failed: "
                    "version=" + std::to_string(v) +
                    " pow=" + std::to_string(pow) +
                    " coinbase=" + std::to_string(coinbase) +
                    " merkle=" + std::to_string(merkle) +
                    " witness=" + std::to_string(witness) +
                    " height=" + std::to_string(block.header.height) +
                    " difficulty=" + std::to_string(block.header.difficulty));
            }
        } else {
            if (!block.validate_against_chain(chain))
                throw std::runtime_error(
                    "block consensus validation failed");
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

    static void write_u32(
        std::ofstream& out,
        std::uint32_t value) {

        for (unsigned i = 0; i < 4; ++i)
            out.put(
                static_cast<char>(
                    (value >> (i * 8)) & 0xffU));

        if (!out)
            throw std::runtime_error(
                "failed writing uint32");
    }

    static void write_u64(
        std::ofstream& out,
        std::uint64_t value) {

        for (unsigned i = 0; i < 8; ++i)
            out.put(
                static_cast<char>(
                    (value >> (i * 8)) & 0xffULL));

        if (!out)
            throw std::runtime_error(
                "failed writing uint64");
    }

    static std::uint32_t read_u32(
        std::ifstream& in) {

        std::uint32_t value = 0;

        for (unsigned i = 0; i < 4; ++i) {
            const int c = in.get();

            if (c == std::ifstream::traits_type::eof())
                throw std::runtime_error(
                    "unexpected end of blockchain storage");

            value |=
                static_cast<std::uint32_t>(
                    static_cast<unsigned char>(c))
                << (i * 8);
        }

        return value;
    }

    static std::uint64_t read_u64(
        std::ifstream& in) {

        std::uint64_t value = 0;

        for (unsigned i = 0; i < 8; ++i) {
            const int c = in.get();

            if (c == std::ifstream::traits_type::eof())
                throw std::runtime_error(
                    "unexpected end of blockchain storage");

            value |=
                static_cast<std::uint64_t>(
                    static_cast<unsigned char>(c))
                << (i * 8);
        }

        return value;
    }

    static void write_block(
        std::ofstream& out,
        const Block& block) {

        const auto data = block.serialize_full_binary();

        if (data.empty() ||
            data.size() > MAX_BLOCK_BYTES) {

            throw std::runtime_error(
                "block exceeds storage size limit");
        }

        if (data.size() > std::numeric_limits<std::uint32_t>::max())
            throw std::runtime_error(
                "block is too large for storage format");

        const auto checksum =
            sha256(bytes_to_binary_string(data));

        write_u32(
            out,
            static_cast<std::uint32_t>(data.size()));

        out.write(
            reinterpret_cast<const char*>(data.data()),
            static_cast<std::streamsize>(data.size()));

        for (const auto byte : checksum)
            out.put(static_cast<char>(byte));

        if (!out)
            throw std::runtime_error(
                "failed writing blockchain block");
    }

    static Block read_block(
        std::ifstream& in) {

        const auto size = read_u32(in);

        if (size == 0 || size > MAX_BLOCK_BYTES)
            throw std::runtime_error(
                "invalid stored block size");

        std::vector<std::uint8_t> data(size);

        in.read(
            reinterpret_cast<char*>(data.data()),
            static_cast<std::streamsize>(data.size()));

        if (in.gcount() !=
            static_cast<std::streamsize>(data.size())) {

            throw std::runtime_error(
                "truncated stored block");
        }

        Hash256 stored_checksum{};

        in.read(
            reinterpret_cast<char*>(stored_checksum.data()),
            static_cast<std::streamsize>(stored_checksum.size()));

        if (in.gcount() !=
            static_cast<std::streamsize>(stored_checksum.size())) {

            throw std::runtime_error(
                "truncated stored block checksum");
        }

        const auto actual_checksum =
            sha256(bytes_to_binary_string(data));

        if (stored_checksum != actual_checksum)
            throw std::runtime_error(
                "stored block checksum mismatch");

        return Block::deserialize_full(data);
    }
};

} // namespace caesar
