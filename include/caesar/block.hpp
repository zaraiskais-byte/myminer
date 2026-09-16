#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <caesar/coinbase.hpp>
#include <caesar/consensus.hpp>
#include <caesar/crypto.hpp>
#include <caesar/serialization.hpp>
#include <caesar/transaction.hpp>

namespace caesar {

struct BlockHeader {
    std::uint32_t version{1};
    std::uint64_t height{0};
    Hash256 previous_hash{};
    Hash256 merkle_root{};
    Hash256 witness_root{};
    std::uint64_t timestamp{0};
    std::uint64_t nonce{0};
    std::uint32_t difficulty{0};

    std::vector<std::uint8_t> serialize_binary() const {
        BinaryWriter writer;

        writer.write_u32(version);
        writer.write_u64(height);

        for (std::uint8_t byte : previous_hash)
            writer.write_u8(byte);

        for (std::uint8_t byte : merkle_root)
            writer.write_u8(byte);

        for (std::uint8_t byte : witness_root)
            writer.write_u8(byte);

        writer.write_u64(timestamp);
        writer.write_u64(nonce);
        writer.write_u32(difficulty);

        return writer.data();
    }

    Hash256 hash() const {
        return sha256(
            bytes_to_binary_string(
                serialize_binary()));
    }
};

struct Block {
    BlockHeader header;
    std::vector<Transaction> transactions;

    Hash256 calculate_merkle_root() const {
        std::vector<Hash256> txids;
        txids.reserve(transactions.size());

        for (const auto& tx : transactions)
            txids.push_back(tx.txid());

        return merkle_root(std::move(txids));
    }

    Hash256 calculate_witness_root() const {
        std::vector<Hash256> witness_ids;
        witness_ids.reserve(transactions.size());

        for (const auto& tx : transactions) {
            witness_ids.push_back(tx.wtxid());
        }

        return merkle_root(std::move(witness_ids));
    }

    void update_merkle_root() {
        header.merkle_root =
            calculate_merkle_root();

        header.witness_root =
            calculate_witness_root();
    }

    bool validate_witness_root() const {
        return header.witness_root ==
               calculate_witness_root();
    }

    std::vector<std::uint8_t> serialize_binary() const {
        BinaryWriter writer;

        writer.write_bytes(
            header.serialize_binary());

        if (transactions.size() >
            static_cast<std::size_t>(
                std::numeric_limits<std::uint32_t>::max())) {

            throw std::runtime_error(
                "too many block transactions");
        }

        writer.write_u32(
            static_cast<std::uint32_t>(
                transactions.size()));

        for (const auto& tx : transactions)
            writer.write_bytes(
                tx.serialize_binary());

        return writer.data();
    }

    std::vector<std::uint8_t> serialize_full_binary() const {
        BinaryWriter writer;
        writer.write_bytes(header.serialize_binary());

        if (transactions.size() >
            static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()))
            throw std::runtime_error("too many block transactions");

        writer.write_u32(static_cast<std::uint32_t>(transactions.size()));

        for (const auto& tx : transactions) {
            const auto serialized_tx =
                tx.serialize_full_binary();

            if (serialized_tx.size() >
                static_cast<std::size_t>(
                    std::numeric_limits<std::uint32_t>::max()))
                throw std::runtime_error(
                    "serialized transaction too large");

            writer.write_u32(
                static_cast<std::uint32_t>(
                    serialized_tx.size()));

            writer.write_bytes(serialized_tx);
        }

        return writer.data();
    }

    static Block deserialize_full(
        const std::vector<std::uint8_t>& data) {

        BinaryReader reader(data);
        Block block;

        block.header.version = reader.read_u32();
        block.header.height = reader.read_u64();

        for (std::uint8_t& byte : block.header.previous_hash)
            byte = reader.read_u8();

        for (std::uint8_t& byte : block.header.merkle_root)
            byte = reader.read_u8();

        for (std::uint8_t& byte : block.header.witness_root)
            byte = reader.read_u8();

        block.header.timestamp = reader.read_u64();
        block.header.nonce = reader.read_u64();
        block.header.difficulty = reader.read_u32();

        const std::uint32_t transaction_count =
            reader.read_u32();

        if (transaction_count > 1000000)
            throw std::runtime_error(
                "too many block transactions");

        block.transactions.reserve(transaction_count);

        for (std::uint32_t i = 0;
             i < transaction_count;
             ++i) {

            const std::uint32_t transaction_size =
                reader.read_u32();

            if (transaction_size > 1000000)
                throw std::runtime_error(
                    "serialized transaction too large");

            const auto transaction_data =
                reader.read_bytes(transaction_size);

            block.transactions.push_back(
                Transaction::deserialize_full(
                    transaction_data));
        }

        if (!reader.empty())
            throw std::runtime_error(
                "trailing bytes after full block");

        return block;
    }

    Hash256 hash() const {
        return header.hash();
    }

    std::vector<std::uint8_t> pow_header() const {
        BlockHeader mining_header = header;
        mining_header.nonce = 0;
        return mining_header.serialize_binary();
    }

    Hash256 pow_hash() const {
        return calculate_pow_hash(pow_header(), header.nonce);
    }

    bool validate_pow() const {
        return caesar::validate_pow(
            pow_header(),
            header.nonce,
            header.difficulty);
    }

    bool validate_merkle_root() const {
        return header.merkle_root ==
               calculate_merkle_root();
    }


    bool validate_difficulty_against_history(
        std::uint32_t previous_difficulty,
        const std::vector<std::uint64_t>& intervals) const {

        const std::uint32_t expected =
            adjust_difficulty_window(
                previous_difficulty,
                intervals);

        return header.difficulty == expected;
    }

    bool validate_against_history(
        std::uint32_t previous_difficulty,
        const std::vector<std::uint64_t>& intervals) const {

        if (!validate_basic())
            return false;

        return validate_difficulty_against_history(
            previous_difficulty,
            intervals);
    }

    bool validate_against_chain_history(
        const std::vector<Block>& chain) const {

        if (chain.size() < CZR_DIFFICULTY_WINDOW + 1)
            return false;

        if (!validate_basic())
            return false;

        const std::size_t previous_index = chain.size() - 1;

        const Block& previous = chain[previous_index];

        std::vector<std::uint64_t> intervals;
        intervals.reserve(CZR_DIFFICULTY_WINDOW);

        const std::size_t first =
            previous_index + 1 - CZR_DIFFICULTY_WINDOW;

        for (std::size_t i = first;
             i <= previous_index;
             ++i) {

            if (i == 0)
                return false;

            const std::uint64_t current_time =
                chain[i].header.timestamp;

            const std::uint64_t previous_time =
                chain[i - 1].header.timestamp;

            if (current_time < previous_time)
                return false;

            intervals.push_back(
                current_time - previous_time);
        }

        return validate_difficulty_against_history(
            previous.header.difficulty,
            intervals);
    }




    bool validate_against_chain(
        const std::vector<Block>& chain) const {

        if (chain.empty())
            return false;

        const Block& previous = chain.back();

        if (header.height != previous.header.height + 1)
            return false;

        if (header.previous_hash != previous.hash())
            return false;

        if (header.timestamp < previous.header.timestamp)
            return false;

        if (chain.size() < CZR_DIFFICULTY_WINDOW + 1)
            return false;

        std::vector<std::uint64_t> intervals;
        intervals.reserve(CZR_DIFFICULTY_WINDOW);

        const std::size_t previous_index =
            chain.size() - 1;

        const std::size_t first =
            previous_index + 1 - CZR_DIFFICULTY_WINDOW;

        for (std::size_t i = first;
             i <= previous_index;
             ++i) {

            if (i == 0)
                return false;

            const std::uint64_t current_time =
                chain[i].header.timestamp;

            const std::uint64_t previous_time =
                chain[i - 1].header.timestamp;

            if (current_time < previous_time)
                return false;

            intervals.push_back(
                current_time - previous_time);
        }

        if (!validate_basic())
            return false;

        return validate_difficulty_against_history(
            previous.header.difficulty,
            intervals);
    }

    bool validate_basic() const {
        if (header.version == 0)
            return false;

        if (!validate_pow())
            return false;

        if (header.height == 0) {
            for (std::uint8_t byte :
                 header.previous_hash) {

                if (byte != 0)
                    return false;
            }

            // Genesis is the only block without a normal
            // protocol coinbase.
            for (const auto& tx : transactions) {
                if (is_coinbase_transaction(tx))
                    return false;

                if (!tx.validate())
                    return false;
            }

            return !transactions.empty() &&
                   validate_merkle_root() && validate_witness_root();
        }

        if (!validate_coinbase_position_and_reward(
                transactions,
                header.height)) {

            return false;
        }

        for (std::size_t i = 1;
             i < transactions.size();
             ++i) {

            if (!transactions[i].validate())
                return false;
        }

        return validate_merkle_root() && validate_witness_root();
    }
};

inline bool validate_block_link(
    const Block& previous,
    const Block& current) {

    if (!previous.validate_basic() ||
        !current.validate_basic()) {

        return false;
    }

    if (current.header.height !=
        previous.header.height + 1) {

        return false;
    }

    if (current.header.previous_hash !=
        previous.hash()) {

        return false;
    }

    return true;
}

inline bool validate_block_chain(
    const std::vector<Block>& chain) {

    if (chain.empty())
        return false;

    const Block& genesis =
        chain.front();

    if (!genesis.validate_basic())
        return false;

    if (genesis.header.height != 0)
        return false;

    for (std::size_t i = 1;
         i < chain.size();
         ++i) {

        if (!validate_block_link(
                chain[i - 1],
                chain[i])) {

            return false;
        }
    }

    return true;
}

} // namespace caesar
