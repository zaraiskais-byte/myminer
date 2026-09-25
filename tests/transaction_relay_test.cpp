#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <thread>
#include <utility>

#include <caesar/node.hpp>
#include <caesar/transaction_signature.hpp>
#include <caesar/wallet.hpp>

using namespace caesar;

static Transaction make_spend(
    const Hash256& funding_txid,
    std::uint32_t output_index,
    std::uint64_t amount,
    const std::string& recipient,
    const Wallet& signer) {

    Transaction tx;

    TransactionInput input;
    input.previous_txid = funding_txid;
    input.output_index = output_index;
    tx.inputs.push_back(input);

    TransactionOutput output;
    output.amount = amount;
    output.recipient = recipient;
    tx.outputs.push_back(output);

    TransactionWitness witness;
    witness.public_key = signer.public_key();
    witness.signature =
        sign_transaction_input(
            tx,
            0,
            signer.private_key());

    tx.witness.inputs.push_back(
        std::move(witness));

    return tx;
}

static bool wait_for_mempool(
    const CaesarNode& node,
    const Hash256& txid,
    int attempts = 100) {

    for (int i = 0; i < attempts; ++i) {
        if (node.mempool().contains(txid))
            return true;

        std::this_thread::sleep_for(
            std::chrono::milliseconds(20));
    }

    return false;
}

int main() {
    std::cout
        << "=== Caesar CZR Transaction Relay Tests ===\n";

    const auto base =
        std::filesystem::temp_directory_path() /
        "caesar_transaction_relay_test";

    std::error_code ec;
    std::filesystem::remove_all(base, ec);

    const auto node_a_dir = base / "node_a";
    const auto node_b_dir = base / "node_b";

    constexpr std::uint16_t port_a = 29444;
    constexpr std::uint16_t port_b = 29445;

    try {
        Wallet alice;
        Wallet bob;

        CaesarNode node_a(
            node_a_dir,
            port_a,
            1);

        CaesarNode node_b(
            node_b_dir,
            port_b,
            1);

        node_a.start();
        node_b.start();

        assert(node_a.running());
        assert(node_b.running());

        /*
         * Create a real spendable UTXO on node A.
         * Install the resulting chain on node B so both
         * nodes have the same UTXO view before relay.
         */
        node_a.mine_one_block(
            alice.address(),
            1000000);

        const auto chain_a = node_a.chain();

        assert(chain_a.size() == 2);
        assert(chain_a.back().header.height == 1);
        assert(validate_block_chain(chain_a));

        assert(node_b.replace_chain(chain_a));

        const auto chain_b = node_b.chain();

        assert(chain_b.size() == 2);
        assert(chain_b.back().hash() ==
               chain_a.back().hash());

        std::cout
            << "[PASS] Both nodes share the same funded chain\n";

        const auto peer_id =
            node_a.connect_to_peer(
                "127.0.0.1",
                port_b);

        assert(peer_id != 0);

        for (int i = 0;
             i < 100 && node_b.peer_count() == 0;
             ++i) {

            std::this_thread::sleep_for(
                std::chrono::milliseconds(20));
        }

        assert(node_a.peer_count() == 1);
        assert(node_b.peer_count() == 1);

        std::cout
            << "[PASS] Node A connected to Node B\n";

        const Block& funding_block =
            chain_a.back();

        assert(!funding_block.transactions.empty());

        const Transaction& coinbase =
            funding_block.transactions.front();

        assert(!coinbase.outputs.empty());

        assert(
            coinbase.outputs.front().recipient ==
            alice.address());

        const Hash256 funding_txid =
            coinbase.txid();

        const std::uint64_t spend_amount =
            coinbase.outputs.front().amount;

        Transaction tx =
            make_spend(
                funding_txid,
                0,
                spend_amount,
                bob.address(),
                alice);

        const Hash256 txid = tx.txid();

        const auto accepted =
            node_a.accept_transaction(tx);

        assert(accepted.accepted());

        std::cout
            << "[PASS] Node A accepted signed transaction\n";

        assert(
            wait_for_mempool(
                node_b,
                txid));

        std::cout
            << "[PASS] Transaction relayed to Node B mempool\n";

        const auto duplicate =
            node_a.accept_transaction(tx);

        assert(!duplicate.accepted());

        assert(
            duplicate.reason ==
            MempoolRejectReason::Duplicate);

        std::cout
            << "[PASS] Duplicate transaction rejected\n";

        Transaction invalid = tx;

        assert(
            !invalid.witness.inputs.empty());

        assert(
            !invalid.witness.inputs.front()
                .signature.empty());

        invalid.witness.inputs.front()
            .signature.front() ^= 0x01;

        const auto invalid_result =
            node_a.accept_transaction(invalid);

        assert(!invalid_result.accepted());

        std::cout
            << "[PASS] Invalid-signature transaction rejected\n";

        assert(node_a.mempool().size() == 1);
        assert(node_b.mempool().size() == 1);
        assert(node_b.mempool().contains(txid));

        std::cout
            << "[PASS] Relay admission state remains consistent\n";

        node_a.stop();
        node_b.stop();

        assert(!node_a.running());
        assert(!node_b.running());

        std::filesystem::remove_all(base, ec);

        std::cout
            << "ALL TRANSACTION RELAY TESTS PASSED\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr
            << "[FAIL] "
            << e.what()
            << '\n';

        std::filesystem::remove_all(base, ec);
        return 1;
    }
}
