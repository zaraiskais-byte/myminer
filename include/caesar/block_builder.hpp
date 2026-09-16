#pragma once

#include <cstdint>
#include <stdexcept>
#include <vector>

#include <caesar/block.hpp>
#include <caesar/mempool.hpp>
#include <caesar/ownership.hpp>
#include <caesar/utxo.hpp>

namespace caesar {

class BlockBuilder {
public:
    static Block build(
        const Block& previous,
        const Mempool& mempool,
        const std::string& miner_recipient,
        std::uint64_t timestamp,
        std::uint32_t difficulty,
        std::uint64_t nonce = 0) {

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
                miner_recipient));

        for (const auto& entry :
             mempool.transactions()) {

            block.transactions.push_back(
                entry.second);
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
        return block.validate_pow();
    }
};

inline UTXOSet apply_block_transactions(
    const Block& block,
    const UTXOSet& previous_utxos) {

    if (!block.validate_basic())
        throw std::runtime_error(
            "cannot apply invalid block");

    UTXOSet result =
        previous_utxos;

    // Coinbase creates new value and therefore is
    // deliberately handled separately from normal
    // UTXO spending.
    const Transaction& coinbase =
        block.transactions.front();

    const Hash256 coinbase_txid =
        coinbase.txid();

    for (std::size_t i = 0;
         i < coinbase.outputs.size();
         ++i) {

        if (!result.add(
                OutPoint{
                    coinbase_txid,
                    static_cast<std::uint32_t>(i)
                },
                coinbase.outputs[i])) {

            throw std::runtime_error(
                "failed to add coinbase UTXO");
        }
    }

    for (std::size_t tx_index = 1;
         tx_index < block.transactions.size();
         ++tx_index) {

        const auto& tx =
            block.transactions[tx_index];

        if (!validate_transaction_witness(
                tx,
                result)) {

            throw std::runtime_error(
                "block transaction failed UTXO validation");
        }

        for (const auto& input :
             tx.inputs) {

            const OutPoint point{
                input.previous_txid,
                input.output_index
            };

            if (!result.spend(point)) {
                throw std::runtime_error(
                    "failed to spend transaction input");
            }
        }

        const Hash256 txid =
            tx.txid();

        for (std::size_t i = 0;
             i < tx.outputs.size();
             ++i) {

            if (i >
                static_cast<std::size_t>(
                    std::numeric_limits<std::uint32_t>::max())) {

                throw std::runtime_error(
                    "transaction output index overflow");
            }

            const OutPoint point{
                txid,
                static_cast<std::uint32_t>(i)
            };

            if (!result.add(
                    point,
                    tx.outputs[i])) {

                throw std::runtime_error(
                    "failed to add transaction output");
            }
        }
    }

    return result;
}


inline bool validate_total_coinbase_issuance(
    const Block& block,
    const std::vector<Block>& chain) {

    std::uint64_t total_issued = 0;

    auto add_issuance = [&](const Block& candidate) -> bool {
        if (candidate.transactions.empty())
            return true;

        if (!is_coinbase_transaction(
                candidate.transactions.front())) {
            return true;
        }

        const auto& coinbase =
            candidate.transactions.front();

        for (const auto& output : coinbase.outputs) {
            if (output.amount >
                CZR_MAX_SUPPLY - total_issued) {
                return false;
            }

            total_issued += output.amount;
        }

        return true;
    };

    for (const auto& previous : chain) {
        if (!add_issuance(previous))
            return false;
    }

    return add_issuance(block);
}

inline bool validate_block_against_utxo(
    const Block& block,
    const UTXOSet& previous_utxos);

inline bool validate_block_consensus(
    const Block& block,
    const std::vector<Block>& chain,
    const UTXOSet& previous_utxos) {

    if (chain.empty())
        return false;

    if (!block.validate_against_chain(chain))
        return false;

    if (!validate_block_against_utxo(
            block,
            previous_utxos)) {
        return false;
    }

    return validate_total_coinbase_issuance(
        block,
        chain);
}

inline bool validate_block_against_utxo(
    const Block& block,
    const UTXOSet& previous_utxos) {

    try {
        (void)apply_block_transactions(
            block,
            previous_utxos);

        return true;
    }
    catch (...) {
        return false;
    }
}

} // namespace caesar
