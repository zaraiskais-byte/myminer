#pragma once

#include <optional>
#include <utility>
#include <vector>

#include <caesar/block.hpp>
#include <caesar/block_builder.hpp>
#include <caesar/chain_work.hpp>
#include <caesar/utxo.hpp>

namespace caesar {

struct ChainReplacementPlan {
    std::vector<Block> chain;
    UTXOSet utxo;
};

// Assemble a candidate fork from an existing canonical prefix and the
// blocks received after a common ancestor.  This function performs only
// structural assembly; full consensus validation belongs to
// prepare_chain_replacement().
inline std::optional<std::vector<Block>> assemble_candidate_chain(
    const std::vector<Block>& current,
    const std::vector<BlockHeader>& headers,
    const std::vector<Block>& blocks) {

    if (current.empty() || headers.empty() || blocks.empty())
        return std::nullopt;

    if (headers.size() != blocks.size())
        return std::nullopt;

    // The first received header must directly follow a block in the
    // current canonical chain.  That block is the common ancestor.
    std::size_t ancestor_index = current.size();

    for (std::size_t i = 0; i < current.size(); ++i) {
        if (current[i].hash() == headers.front().previous_hash) {
            ancestor_index = i;
            break;
        }
    }

    if (ancestor_index == current.size())
        return std::nullopt;

    std::vector<Block> candidate;
    candidate.reserve(ancestor_index + 1 + blocks.size());

    candidate.insert(
        candidate.end(),
        current.begin(),
        current.begin() + ancestor_index + 1);

    Hash256 previous_hash = current[ancestor_index].hash();
    std::uint64_t expected_height =
        current[ancestor_index].header.height + 1;

    for (std::size_t i = 0; i < blocks.size(); ++i) {
        const auto& header = headers[i];
        const auto& block = blocks[i];

        if (header.hash() != block.hash())
            return std::nullopt;

        if (block.header.previous_hash != previous_hash)
            return std::nullopt;

        if (block.header.height != expected_height)
            return std::nullopt;

        if (block.header.hash() != header.hash())
            return std::nullopt;

        candidate.push_back(block);

        previous_hash = block.hash();
        ++expected_height;
    }

    return candidate;
}

// Expensive preparation stage.
// The current canonical state is never modified here.
inline std::optional<ChainReplacementPlan> prepare_chain_replacement(
    const std::vector<Block>& current,
    const std::vector<Block>& candidate) {

    if (candidate.empty())
        return std::nullopt;

    // A replacement must keep the same canonical genesis.
    if (!current.empty() &&
        candidate.front().hash() != current.front().hash()) {
        return std::nullopt;
    }

    // Full consensus validation happens before chain selection.
    if (!validate_candidate_chain(candidate))
        return std::nullopt;

    // Equal work does not replace the current canonical chain.
    if (!current.empty() &&
        !has_more_work(candidate, current)) {
        return std::nullopt;
    }

    // Build the complete candidate UTXO state before committing anything.
    UTXOSet candidate_utxo = rebuild_utxo_set(candidate);

    ChainReplacementPlan plan;
    plan.chain = candidate;
    plan.utxo = std::move(candidate_utxo);

    return plan;
}

// Tiny commit stage.
// All expensive operations must already have succeeded.
inline void commit_chain_replacement(
    std::vector<Block>& canonical_chain,
    UTXOSet& canonical_utxo,
    ChainReplacementPlan&& plan) noexcept {

    canonical_chain.swap(plan.chain);
    canonical_utxo = std::move(plan.utxo);
}

} // namespace caesar
