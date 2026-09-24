#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#include <caesar/block.hpp>
#include <caesar/coinbase.hpp>
#include <caesar/ownership.hpp>
#include <caesar/utxo.hpp>

namespace caesar {

struct ChainValidationResult {
    UTXOSet utxo;
    std::uint64_t total_issued = 0;
};

inline bool apply_block_transactions_canonical(
    const Block& block,
    const UTXOSet& previous_utxos,
    UTXOSet& result) {

    if (!block.validate_basic())
        return false;

    result = previous_utxos;

    if (block.transactions.empty())
        return false;

    const Transaction& coinbase =
        block.transactions.front();

    if (!validate_coinbase_transaction(
            coinbase,
            block.header.height)) {
        return false;
    }

    const Hash256 coinbase_txid =
        coinbase.txid();

    for (std::size_t i = 0;
         i < coinbase.outputs.size();
         ++i) {

        if (i >
            static_cast<std::size_t>(
                std::numeric_limits<std::uint32_t>::max())) {
            return false;
        }

        const OutPoint point{
            coinbase_txid,
            static_cast<std::uint32_t>(i)
        };

        if (!result.add(
                point,
                coinbase.outputs[i])) {
            return false;
        }
    }

    for (std::size_t tx_index = 1;
         tx_index < block.transactions.size();
         ++tx_index) {

        const Transaction& tx =
            block.transactions[tx_index];

        if (is_coinbase_transaction(tx))
            return false;

        if (!validate_transaction_witness(
                tx,
                result)) {
            return false;
        }

        for (const auto& input : tx.inputs) {
            const OutPoint point{
                input.previous_txid,
                input.output_index
            };

            if (!result.spend(point))
                return false;
        }

        const Hash256 txid =
            tx.txid();

        for (std::size_t i = 0;
             i < tx.outputs.size();
             ++i) {

            if (i >
                static_cast<std::size_t>(
                    std::numeric_limits<std::uint32_t>::max())) {
                return false;
            }

            const OutPoint point{
                txid,
                static_cast<std::uint32_t>(i)
            };

            if (!result.add(
                    point,
                    tx.outputs[i])) {
                return false;
            }
        }
    }

    return true;
}

inline bool validate_genesis_canonical(
    const Block& genesis) {

    if (genesis.header.height != 0)
        return false;

    if (genesis.header.previous_hash != Hash256{})
        return false;

    if (genesis.header.difficulty != 0)
        return false;

    return genesis.validate_basic();
}

inline bool validate_block_position_with_chain(
    const std::vector<Block>& chain,
    std::size_t block_index) {

    if (block_index == 0 || block_index >= chain.size())
        return false;

    const Block& previous = chain[block_index - 1];
    const Block& block = chain[block_index];

    if (block.header.height !=
        previous.header.height + 1) {
        return false;
    }

    if (block.header.previous_hash !=
        previous.hash()) {
        return false;
    }

    if (block.header.timestamp <
        previous.header.timestamp) {
        return false;
    }

    if (chain.size() < CZR_DIFFICULTY_WINDOW + 1) {
        const std::uint32_t expected =
            (previous.header.height == 0 &&
             previous.header.difficulty == 0)
                ? CZR_INITIAL_MINING_DIFFICULTY
                : previous.header.difficulty;

        return block.header.difficulty == expected;
    }

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

        const auto current =
            chain[i].header.timestamp;

        const auto previous_time =
            chain[i - 1].header.timestamp;

        if (current < previous_time)
            return false;

        intervals.push_back(
            current - previous_time);
    }

    return block.header.difficulty ==
        adjust_difficulty_window(
            previous.header.difficulty,
            intervals);
}

inline bool add_coinbase_issuance(
    const Block& block,
    std::uint64_t& total_issued) {

    if (block.transactions.empty())
        return true;

    const auto& coinbase =
        block.transactions.front();

    if (!is_coinbase_transaction(coinbase))
        return true;

    for (const auto& output : coinbase.outputs) {
        if (output.amount >
            CZR_MAX_SUPPLY - total_issued) {
            return false;
        }

        total_issued += output.amount;
    }

    return true;
}

inline bool validate_and_rebuild_chain(
    const std::vector<Block>& chain,
    ChainValidationResult& result) {

    if (chain.empty())
        return false;

    if (!validate_genesis_canonical(
            chain.front())) {
        return false;
    }

    UTXOSet utxos;
    std::uint64_t total_issued = 0;

    for (std::size_t i = 1;
         i < chain.size();
         ++i) {

        const Block& previous =
            chain[i - 1];

        const Block& block =
            chain[i];

        if (!block.validate_basic())
            return false;

        if (!validate_block_position_with_chain(
                chain,
                i)) {
            return false;
        }

        if (!add_coinbase_issuance(
                block,
                total_issued)) {
            return false;
        }

        UTXOSet next;

        if (!apply_block_transactions_canonical(
                block,
                utxos,
                next)) {
            return false;
        }

        utxos = std::move(next);
    }

    result.utxo = std::move(utxos);
    result.total_issued = total_issued;

    return true;
}

inline bool validate_candidate_chain(
    const std::vector<Block>& candidate) {

    ChainValidationResult result;

    return validate_and_rebuild_chain(
        candidate,
        result);
}

inline UTXOSet rebuild_utxo_set(
    const std::vector<Block>& chain) {

    ChainValidationResult result;

    if (!validate_and_rebuild_chain(
            chain,
            result)) {
        throw std::runtime_error(
            "cannot rebuild UTXO set from invalid chain");
    }

    return std::move(result.utxo);
}

inline UTXOSet apply_block_transactions(
    const Block& block,
    const UTXOSet& previous_utxos) {

    UTXOSet result;

    if (!apply_block_transactions_canonical(
            block,
            previous_utxos,
            result)) {
        throw std::runtime_error(
            "cannot apply invalid block");
    }

    return result;
}

inline bool validate_total_coinbase_issuance(
    const Block& block,
    const std::vector<Block>& chain) {

    std::uint64_t total_issued = 0;

    for (const auto& previous : chain) {
        if (previous.header.height == 0)
            continue;

        if (!add_coinbase_issuance(
                previous,
                total_issued)) {
            return false;
        }
    }

    return add_coinbase_issuance(
        block,
        total_issued);
}

inline bool validate_block_against_utxo(
    const Block& block,
    const UTXOSet& previous_utxos) {

    UTXOSet result;

    return apply_block_transactions_canonical(
        block,
        previous_utxos,
        result);
}

inline bool validate_block_consensus(
    const Block& block,
    const std::vector<Block>& chain,
    const UTXOSet& previous_utxos) {

    if (chain.empty())
        return false;

    if (!block.validate_basic())
        return false;

    const Block& previous = chain.back();

    if (block.header.height !=
        previous.header.height + 1) {
        return false;
    }

    if (block.header.previous_hash !=
        previous.hash()) {
        return false;
    }

    if (block.header.timestamp <
        previous.header.timestamp) {
        return false;
    }

    std::uint32_t expected_difficulty = 0;

    if (chain.size() < CZR_DIFFICULTY_WINDOW + 1) {
        expected_difficulty =
            (previous.header.height == 0 &&
             previous.header.difficulty == 0)
                ? CZR_INITIAL_MINING_DIFFICULTY
                : previous.header.difficulty;
    } else {
        std::vector<std::uint64_t> intervals;
        intervals.reserve(CZR_DIFFICULTY_WINDOW);

        const std::size_t start =
            chain.size() - CZR_DIFFICULTY_WINDOW - 1;

        for (std::size_t i = start + 1;
             i < chain.size();
             ++i) {
            if (chain[i].header.timestamp <
                chain[i - 1].header.timestamp) {
                return false;
            }

            intervals.push_back(
                chain[i].header.timestamp -
                chain[i - 1].header.timestamp);
        }

        expected_difficulty =
            adjust_difficulty_window(
                chain.back().header.difficulty,
                intervals);
    }

    if (block.header.difficulty !=
        expected_difficulty) {
        return false;
    }

    if (!validate_block_against_utxo(
            block,
            previous_utxos)) {
        return false;
    }

    return validate_total_coinbase_issuance(
        block,
        chain);
}

} // namespace caesar
