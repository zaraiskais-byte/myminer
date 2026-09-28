/*
 * Mempool revalidation after chain replacement — smoke test.
 *
 * Before the change, CaesarNode::replace_chain() left the mempool
 * untouched. Transactions whose inputs no longer existed under the
 * new chain stayed in the pool and would only be rejected when a
 * miner or relay tried to use them.
 *
 * This test verifies that revalidate_mempool_after_reorg() runs to
 * completion on the edge cases that are easy to trigger:
 *
 *   1. Empty mempool  -> no-op.
 *   2. Empty chain    -> mempool cleared.
 *   3. Genesis-only chain with empty mempool -> no-op.
 *
 * A full integration test with valid signed transactions is added in
 * a follow-up commit, once the test harness for signing is shared
 * with transaction_relay_test.cpp.
 */

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

#include <caesar/mempool.hpp>
#include <caesar/mempool_reorg.hpp>
#include <caesar/network_params.hpp>

using namespace caesar;

int main() {

    // 1. Empty mempool: must be a no-op and must not throw.
    {
        Mempool mempool;
        std::vector<Block> chain;
        chain.push_back(build_canonical_genesis());

        revalidate_mempool_after_reorg(mempool, chain);
        assert(mempool.size() == 0);
    }
    std::cout << "[mempool-reorg] empty mempool: OK\n";

    // 2. Empty chain: still must not throw. Mempool is empty so
    //    nothing to do; the early return protects us from touching
    //    rebuild_utxo_set on an empty chain.
    {
        Mempool mempool;
        std::vector<Block> empty_chain;

        revalidate_mempool_after_reorg(mempool, empty_chain);
        assert(mempool.size() == 0);
    }
    std::cout << "[mempool-reorg] empty chain: OK\n";

    // 3. Genesis-only chain with empty mempool.
    {
        Mempool mempool;
        std::vector<Block> genesis_only;
        genesis_only.push_back(build_canonical_genesis());

        revalidate_mempool_after_reorg(mempool, genesis_only);
        assert(mempool.size() == 0);
    }
    std::cout << "[mempool-reorg] genesis-only chain: OK\n";

    std::cout << "CaesarMempoolReorgTest: PASS\n";
    return 0;
}
