#include <cstdlib>
#include <iostream>
#include <string>

#include <caesar/mempool.hpp>
#include <caesar/transaction_signature.hpp>
#include <caesar/wallet.hpp>

using namespace caesar;

static Transaction make_transaction(
    const Hash256& funding_txid,
    std::uint64_t amount,
    const std::string& recipient,
    const Wallet& signer) {

    Transaction tx;

    TransactionInput input;
    input.previous_txid = funding_txid;
    input.output_index = 0;

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

int main() {
    std::cout
        << "=== Caesar CZR Mempool Tests ===\n";

    try {
        Wallet alice;
        Wallet bob;

        UTXOSet utxos;

        const Hash256 funding_txid =
            sha256("Caesar mempool real funding");

        const OutPoint funded{
            funding_txid,
            0
        };

        TransactionOutput funding;
        funding.amount = 5000000;
        funding.recipient = alice.address();

        if (!utxos.add(funded, funding)) {
            std::cerr
                << "[FAIL] Funding UTXO creation\n";
            return EXIT_FAILURE;
        }

        Mempool mempool;

        Transaction tx1 =
            make_transaction(
                funding_txid,
                4000000,
                bob.address(),
                alice);

        const auto result1 =
            mempool.accept(
                tx1,
                utxos);

        if (!result1.accepted()) {
            std::cerr
                << "[FAIL] First transaction rejected: "
                << mempool_reject_reason_string(
                       result1.reason)
                << '\n';

            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] First transaction accepted\n";

        if (mempool.size() != 1) {
            std::cerr
                << "[FAIL] Mempool size after first transaction\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Mempool contains first transaction\n";

        const OutPoint spent{
            funding_txid,
            0
        };

        if (!mempool.input_reserved(spent)) {
            std::cerr
                << "[FAIL] Input was not reserved\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] UTXO input reserved\n";

        Transaction tx2 =
            make_transaction(
                funding_txid,
                3000000,
                bob.address(),
                alice);

        const auto result2 =
            mempool.accept(
                tx2,
                utxos);

        if (result2.reason !=
            MempoolRejectReason::DoubleSpend) {

            std::cerr
                << "[FAIL] Double spend was not rejected: "
                << mempool_reject_reason_string(
                       result2.reason)
                << '\n';

            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Double-spend transaction rejected\n";

        if (mempool.size() != 1) {
            std::cerr
                << "[FAIL] Double-spend changed mempool\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Double-spend did not alter mempool\n";

        const Hash256 tx1_id =
            tx1.txid();

        if (!mempool.contains(tx1_id)) {
            std::cerr
                << "[FAIL] Stored transaction not found\n";
            return EXIT_FAILURE;
        }

        if (!mempool.get(tx1_id)) {
            std::cerr
                << "[FAIL] Stored transaction unavailable\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Stored transaction retrievable by TXID\n";

        if (!mempool.remove(tx1_id)) {
            std::cerr
                << "[FAIL] Transaction removal failed\n";
            return EXIT_FAILURE;
        }

        if (mempool.size() != 0) {
            std::cerr
                << "[FAIL] Mempool not empty after removal\n";
            return EXIT_FAILURE;
        }

        if (mempool.input_reserved(spent)) {
            std::cerr
                << "[FAIL] Input reservation remained after removal\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Transaction removal releases input\n";

        const auto result3 =
            mempool.accept(
                tx2,
                utxos);

        if (!result3.accepted()) {
            std::cerr
                << "[FAIL] Transaction rejected after reservation release: "
                << mempool_reject_reason_string(
                       result3.reason)
                << '\n';

            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Released input can be used again\n";

        mempool.clear();

        if (mempool.size() != 0) {
            std::cerr
                << "[FAIL] Mempool clear failed\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Mempool clear successful\n";

        std::cout
            << "ALL MEMPOOL TESTS PASSED\n";

        return EXIT_SUCCESS;

    } catch (const std::exception& e) {
        std::cerr
            << "[FAIL] "
            << e.what()
            << '\n';

        return EXIT_FAILURE;
    }
}
