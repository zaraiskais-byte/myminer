#pragma once

#include <cstdint>
#include <stdexcept>
#include <vector>

#include <caesar/block.hpp>
#include <caesar/mempool.hpp>
#include <caesar/chain_validator.hpp>

namespace caesar {

class BlockBuilder {
public:
    static Block build(
        const Block& previous,
        const Mempool& mempool,
        const std::string& miner_recipient,
        std::uint64_t timestamp,
        std::uint32_t difficulty,
        std::uint64_t nonce = 0,
        const UTXOSet* chain_utxos = nullptr) {

        if (miner_recipient.empty())
            throw std::runtime_error(
                "miner recipient is empty");

        Block block;

        block.header.version = 1;
        block.header.height =
            previous.header.height + 1;
        block.header.previous_hash =
            previous.hash();
        block.header.timestamp =
            timestamp;
        block.header.difficulty =
            difficulty;
        block.header.nonce =
            nonce;

        // Coinbase MUST be transaction zero.
        block.transactions.push_back(
            make_coinbase_transaction(
                block.header.height,
                miner_recipient,
                block.header.height));

        if (chain_utxos) {
            const auto ordered =
                mempool.ordered_transactions(
                    *chain_utxos);

            for (const auto& tx : ordered)
                block.transactions.push_back(tx);
        } else {
            for (const auto& entry :
                 mempool.transactions()) {

                block.transactions.push_back(
                    entry.second);
            }
        }

        block.update_merkle_root();

        return block;
    }

    static bool mine(
        Block& block,
        std::uint64_t start_nonce,
        std::uint64_t max_attempts) {

        if (block.header.difficulty > 256)
            return false;

        std::uint64_t found_nonce = 0;
        Hash256 found_hash{};

        const bool found = mine_pow(
            block.pow_header(),
            block.header.difficulty,
            start_nonce,
            max_attempts,
            found_nonce,
            found_hash);

        if (!found)
            return false;

        block.header.nonce = found_nonce;

        if (block.pow_hash() != found_hash)
            throw std::runtime_error(
                "PoW hash changed immediately after mining");

        return block.validate_pow();
    }
};

} // namespace caesar
