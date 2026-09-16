#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <caesar/transaction_signature.hpp>
#include <caesar/wallet.hpp>

using namespace caesar;

static Transaction make_transaction(
    const Hash256& previous_txid) {

    Transaction tx;

    TransactionInput input;
    input.previous_txid = previous_txid;
    input.output_index = 0;

    tx.inputs.push_back(input);

    TransactionOutput output;
    output.amount = 1000000;
    output.recipient = "CZ1-test-destination";

    tx.outputs.push_back(output);

    return tx;
}

int main() {
    std::cout
        << "=== Caesar CZR Transaction Signature Tests ===\n";

    try {
        Wallet alice;

        const Hash256 funding_txid =
            sha256("Caesar signature funding");

        Transaction tx =
            make_transaction(funding_txid);

        const auto signature =
            sign_transaction_input(
                tx,
                0,
                alice.private_key());

        if (signature.empty()) {
            std::cerr
                << "[FAIL] Empty transaction signature\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Binary transaction signature generated\n";

        if (!verify_transaction_input_signature(
                tx,
                0,
                alice.public_key_handle(),
                signature)) {

            std::cerr
                << "[FAIL] Valid binary signature rejected\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Binary transaction signature verified\n";

        Transaction changed_amount = tx;
        changed_amount.outputs[0].amount++;

        if (verify_transaction_input_signature(
                changed_amount,
                0,
                alice.public_key_handle(),
                signature)) {

            std::cerr
                << "[FAIL] Modified amount accepted\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Modified amount rejected\n";

        Transaction changed_recipient = tx;
        changed_recipient.outputs[0].recipient =
            "CZ1-modified-destination";

        if (verify_transaction_input_signature(
                changed_recipient,
                0,
                alice.public_key_handle(),
                signature)) {

            std::cerr
                << "[FAIL] Modified recipient accepted\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Modified recipient rejected\n";

        Transaction changed_input = tx;
        changed_input.inputs[0].output_index = 1;

        if (verify_transaction_input_signature(
                changed_input,
                0,
                alice.public_key_handle(),
                signature)) {

            std::cerr
                << "[FAIL] Modified input accepted\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Modified input rejected\n";

        Wallet attacker;

        if (verify_transaction_input_signature(
                tx,
                0,
                attacker.public_key_handle(),
                signature)) {

            std::cerr
                << "[FAIL] Wrong public key accepted\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Wrong public key rejected\n";

        std::cout
            << "ALL TRANSACTION SIGNATURE TESTS PASSED\n";

        return EXIT_SUCCESS;

    } catch (const std::exception& e) {
        std::cerr
            << "[FAIL] "
            << e.what()
            << '\n';

        return EXIT_FAILURE;
    }
}
