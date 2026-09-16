#include <cstdlib>
#include <iostream>

#include <caesar/block_builder.hpp>
#include <caesar/mempool.hpp>
#include <caesar/block.hpp>
#include <caesar/wallet.hpp>
#include <caesar/transaction_signature.hpp>

using namespace caesar;

static Transaction make_spend(
    const Hash256& previous_txid,
    std::uint64_t amount,
    const std::string& recipient) {

    Transaction tx;

    TransactionInput input;
    input.previous_txid = previous_txid;
    input.output_index = 0;

    tx.inputs.push_back(input);

    TransactionOutput output;
    output.amount = amount;
    output.recipient = recipient;

    tx.outputs.push_back(output);

    return tx;
}

int main() {
    try {
        std::cout
            << "=== Caesar CZR Block Builder Tests ===\n";

        Wallet alice;
        Wallet bob;

        UTXOSet utxos;

        Hash256 funding_txid =
            sha256("funding transaction");

        OutPoint funding_point{
            funding_txid,
            0
        };

        TransactionOutput funding_output;
        funding_output.amount = 1000;
        funding_output.recipient =
            alice.address();

        if (!utxos.add(
                funding_point,
                funding_output)) {

            std::cerr
                << "[FAIL] Could not create funding UTXO\n";
            return EXIT_FAILURE;
        }

        Transaction tx =
            make_spend(
                funding_txid,
                900,
                bob.address());

        const auto signature =
            sign_transaction_input(
                tx,
                0,
                alice.private_key());

        tx.witness.inputs.push_back(
            TransactionWitness{
                alice.public_key(),
                signature
            });

        Mempool mempool;

        auto accepted =
            mempool.accept(
                tx,
                utxos);

        if (!accepted.accepted()) {
            std::cerr
                << "[FAIL] Valid transaction rejected by mempool\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Transaction accepted by mempool\n";

        Block previous;

        previous.header.version = 1;
        previous.header.height = 0;
        previous.header.timestamp = 1000;
        previous.header.difficulty = 1;

        Transaction genesis_tx =
            make_spend(
                sha256("genesis input"),
                1,
                "CZ1-genesis");

        previous.transactions.push_back(
            genesis_tx);

        previous.update_merkle_root();

        std::uint64_t previous_nonce = 0;
        Hash256 previous_pow_hash{};

        if (!mine_pow(
                previous.pow_header(),
                previous.header.difficulty,
                0,
                100000,
                previous_nonce,
                previous_pow_hash)) {
            std::cerr
                << "[FAIL] Could not mine previous block\\n";
            return EXIT_FAILURE;
        }

        previous.header.nonce = previous_nonce;

        if (!previous.validate_basic()) {
            std::cerr
                << "[FAIL] Previous block invalid\n";
            return EXIT_FAILURE;
        }

        Block candidate =
            BlockBuilder::build(
                previous,
                mempool,
                "CZ1-miner",
                1060,
                1,
                7);

        if (candidate.transactions.size() != 2) {
            std::cerr
                << "[FAIL] Mempool transaction not copied to block\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Mempool transaction copied to candidate block\n";

        if (candidate.header.height != 1) {
            std::cerr
                << "[FAIL] Candidate height incorrect\n";
            return EXIT_FAILURE;
        }

        if (candidate.header.previous_hash !=
            previous.hash()) {

            std::cerr
                << "[FAIL] Candidate previous hash incorrect\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Candidate block linked correctly\n";

        if (!BlockBuilder::mine(candidate, 0, 100000)) {
            std::cerr << "[FAIL] Could not mine candidate block\n";
            return EXIT_FAILURE;
        }

        if (!candidate.validate_basic()) {
            std::cerr
                << "[FAIL] Candidate block failed basic validation\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Candidate block basic validation succeeded\n";

        if (!validate_block_against_utxo(
                candidate,
                utxos)) {

            std::cerr
                << "[FAIL] Candidate block failed UTXO validation\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Candidate block passed UTXO validation\n";

        Block mined = BlockBuilder::build(
            previous,
            mempool,
            "CZ1-miner",
            1060,
            4,
            0);

        if (!BlockBuilder::mine(
                mined,
                0,
                100000)) {
            std::cerr
                << "[FAIL] BlockBuilder mining failed\\n";
            return EXIT_FAILURE;
        }

        if (!mined.validate_pow()) {
            std::cerr
                << "[FAIL] Mined block PoW is invalid\\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] BlockBuilder mined a valid block\\n";

        UTXOSet next =
            apply_block_transactions(
                candidate,
                utxos);

        if (next.contains(funding_point)) {
            std::cerr
                << "[FAIL] Spent UTXO still exists\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Spent UTXO removed\n";

        OutPoint new_output{
            tx.txid(),
            0
        };

        const TransactionOutput*
            created = next.get(new_output);

        if (!created ||
            created->amount != 900 ||
            created->recipient != bob.address()) {

            std::cerr
                << "[FAIL] New UTXO not created correctly\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] New UTXO created correctly\n";

        Block tampered = candidate;

        tampered.transactions[0]
            .outputs[0]
            .amount = 901;

        if (validate_block_against_utxo(
                tampered,
                utxos)) {

            std::cerr
                << "[FAIL] Tampered block accepted\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Tampered block rejected\n";

        std::cout
            << "ALL BLOCK BUILDER TESTS PASSED\n";

        return EXIT_SUCCESS;

    } catch (const std::exception& e) {
        std::cerr
            << "[FAIL] "
            << e.what()
            << '\n';

        return EXIT_FAILURE;
    }
}
