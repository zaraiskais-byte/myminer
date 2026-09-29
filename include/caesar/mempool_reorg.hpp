#pragma once

#include <cstdint>
#include <set>
#include <vector>

#include <caesar/block.hpp>
#include <caesar/chain_validator.hpp>
#include <caesar/mempool.hpp>
#include <caesar/transaction.hpp>

namespace caesar {

/*
 * Revalidates the mempool against a new canonical chain.
 *
 * When a chain replacement changes the UTXO set, transactions that
 * were valid under the old chain may become invalid: their inputs
 * may no longer exist, their parents may have been removed, or they
 * may now double-spend each other. Historically the mempool kept
 * them unchanged, which left miners and relays to discover the
 * problem only when they tried to build or forward a block.
 *
 * This function performs an eager revalidation:
 *
 *   1. Take a snapshot of every transaction currently in the pool.
 *   2. Clear the pool so its internal state (transactions_,
 *      reserved_inputs_, mempool_bytes_) is reset.
 *   3. Rebuild the UTXO set from the new chain.
 *   4. Reinsert the snapshot in dependency order (parents before
 *      children). A transaction is only retried after all of its
 *      parents have either been accepted or definitively dropped.
 *   5. Any transaction that fails admission under the new chain is
 *      silently discarded. Its descendants are discarded too
 *      because their inputs will never resolve.
 *
 * The function does not throw. It is safe to call with an empty
 * mempool or an empty chain.
 *
 * Note: this is a "drop invalid" policy, not Bitcoin's "reinsert
 * disconnected block transactions" policy. Once the reorg pipeline
 * records which transactions were removed from the old chain, a
 * follow-up can feed them back here before or after this call.
 */
inline void revalidate_mempool_after_reorg(Mempool& mempool, const std::vector<Block>& new_chain) {
    if (mempool.size() == 0)
        return;

    // 1. Snapshot every transaction currently in the pool.
    std::vector<Transaction> snapshot;
    snapshot.reserve(mempool.size());

    for (const auto& entry : mempool.transactions())
        snapshot.push_back(entry.second);

    // 2. Clear so accept() starts from a clean state.
    mempool.clear();

    if (snapshot.empty())
        return;

    // 3. Rebuild UTXO from the new chain.
    UTXOSet new_utxos;
    try {
        new_utxos = rebuild_utxo_set(new_chain);
    } catch (const std::exception&) {
        // New chain is structurally invalid — do not reinsert
        // anything. Callers must already have validated the chain
        // before reaching this point, so this is defensive only.
        return;
    }

    // 4. Topological reinsertion.
    //
    // pending contains the txids that have not yet been accepted or
    // definitively dropped. On each pass we accept every transaction
    // whose parents are no longer pending. Accepting a transaction
    // registers its outputs in the mempool's effective UTXO, so
    // children see them on the next pass.
    std::set<Hash256> pending;
    for (const auto& tx : snapshot)
        pending.insert(tx.txid());

    bool progress = true;

    while (progress && !pending.empty()) {
        progress = false;

        for (const auto& tx : snapshot) {
            const Hash256 id = tx.txid();

            if (pending.find(id) == pending.end())
                continue;

            bool deps_ready = true;

            for (const auto& input : tx.inputs) {
                if (pending.find(input.previous_txid) != pending.end()) {
                    deps_ready = false;
                    break;
                }
            }

            if (!deps_ready)
                continue;

            // accept() may reject due to missing UTXO, signature,
            // size, or policy. Either way the transaction is gone
            // from the pool.
            (void)mempool.accept(tx, new_utxos);
            pending.erase(id);
            progress = true;
        }
    }

    // 5. Anything left in pending is part of a cycle or references
    //    a parent that was itself dropped. It is not reinserted.
}

} // namespace caesar
