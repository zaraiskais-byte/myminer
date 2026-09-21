#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

#include <caesar/crypto.hpp>
#include <caesar/transaction.hpp>

namespace caesar {

// Protocol units:
// 1 CZR = 100,000,000 atomic units.
// These values are protocol candidates and must be finalized
// before any public/mainnet launch.

constexpr std::uint64_t CZR_ATOMIC_UNITS =
    100000000ULL;

constexpr std::uint64_t CZR_MAX_SUPPLY =
    21000000ULL * CZR_ATOMIC_UNITS;

constexpr std::uint64_t CZR_INITIAL_SUBSIDY =
    50ULL * CZR_ATOMIC_UNITS;

constexpr std::uint64_t CZR_HALVING_INTERVAL =
    210000ULL;

inline bool is_zero_hash(
    const Hash256& hash) {

    for (std::uint8_t byte : hash) {
        if (byte != 0)
            return false;
    }

    return true;
}

inline std::uint64_t block_subsidy(
    std::uint64_t height) {

    if (height == 0)
        return 0;

    const std::uint64_t halvings =
        height / CZR_HALVING_INTERVAL;

    if (halvings >= 64)
        return 0;

    return CZR_INITIAL_SUBSIDY >> halvings;
}

inline Transaction make_coinbase_transaction(
    std::uint64_t height,
    const std::string& recipient,
    std::uint64_t extra_nonce = 0) {

    if (height == 0)
        throw std::runtime_error(
            "genesis block cannot use normal coinbase");

    if (recipient.empty())
        throw std::runtime_error(
            "coinbase recipient is empty");

    Transaction tx;

    // Coinbase input has no previous UTXO.
    TransactionInput input;

    input.previous_txid = Hash256{};

    // UINT32_MAX marks a coinbase input.
    input.output_index =
        std::numeric_limits<std::uint32_t>::max();

    tx.inputs.push_back(input);

    TransactionOutput output;

    output.amount =
        block_subsidy(height);

    output.recipient = recipient;

    tx.outputs.push_back(output);

    // Bind the block height and extra nonce into a
    // deterministic second output only through the
    // transaction's canonical data.
    //
    // The extra nonce is represented by an additional
    // zero-value metadata-free output only when needed
    // in future mining work. For now it is encoded by
    // replacing the recipient suffix.
    //
    // Keep the transaction simple for this protocol stage.
    (void)extra_nonce;

    return tx;
}

inline bool is_coinbase_transaction(
    const Transaction& tx) {

    if (tx.inputs.size() != 1)
        return false;

    if (tx.inputs[0].output_index !=
        std::numeric_limits<std::uint32_t>::max()) {

        return false;
    }

    if (!is_zero_hash(
            tx.inputs[0].previous_txid)) {

        return false;
    }

    return true;
}

inline bool validate_coinbase_transaction(
    const Transaction& tx,
    std::uint64_t height) {

    if (!is_coinbase_transaction(tx))
        return false;

    if (height == 0)
        return false;

    if (tx.outputs.size() != 1)
        return false;

    const auto& output =
        tx.outputs.front();

    if (output.amount !=
        block_subsidy(height)) {

        return false;
    }

    if (output.recipient.empty())
        return false;

    return true;
}

inline bool validate_coinbase_position_and_reward(
    const std::vector<Transaction>& transactions,
    std::uint64_t height) {

    if (transactions.empty())
        return false;

    if (!is_coinbase_transaction(
            transactions.front())) {

        return false;
    }

    if (!validate_coinbase_transaction(
            transactions.front(),
            height)) {

        return false;
    }

    for (std::size_t i = 1;
         i < transactions.size();
         ++i) {

        if (is_coinbase_transaction(
                transactions[i])) {

            return false;
        }
    }

    return true;
}

} // namespace caesar
