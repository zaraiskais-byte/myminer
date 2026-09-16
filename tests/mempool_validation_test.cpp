#include <cstdlib>
#include <iostream>
#include <string>

#include <caesar/mempool_validation.hpp>
#include <caesar/wallet.hpp>

using namespace caesar;

static Transaction make_transaction(
    const Hash256& funding_txid,
    std::uint64_t amount,
    const std::string& recipient) {

    Transaction tx;

    TransactionInput input;
    input.previous_txid = funding_txid;
    input.output_index = 0;

    tx.inputs.push_back(input);

    TransactionOutput output;
    output.amount = amount;
    output.recipient = recipient;

    tx.outputs.push_back(output);

    return tx;
}

static bool expect_rejection(
    const Transaction& tx,
    const UTXOSet& utxos,
    MempoolRejectReason expected,
    const std::string& label) {

    const auto result =
        validate_for_mempool(tx, utxos);

    if (result.reason != expected) {
        std::cerr
            << "[FAIL] "
            << label
            << " — got: "
            << mempool_reject_reason_string(result.reason)
            << '\n';

        return false;
    }

    std::cout
        << "[PASS] "
        << label
        << '\n';

    return true;
}

int main() {
    std::cout
        << "=== Caesar CZR Mempool Validation Tests ===\n";

    try {
        Wallet owner;

        UTXOSet utxos;

        const Hash256 funding_txid =
            sha256("Caesar mempool funding");

        const OutPoint funded{
            funding_txid,
            0
        };

        TransactionOutput funding_output;
        funding_output.amount = 5000000;
        funding_output.recipient = owner.address();

        if (!utxos.add(
                funded,
                funding_output)) {

            std::cerr
                << "[FAIL] Could not create funding UTXO\n";

            return EXIT_FAILURE;
        }

        Transaction valid =
            make_transaction(
                funding_txid,
                4000000,
                owner.address());

        const auto valid_result =
            validate_for_mempool(
                valid,
                utxos);

        if (!valid_result.accepted()) {
            std::cerr
                << "[FAIL] Valid transaction rejected: "
                << mempool_reject_reason_string(
                       valid_result.reason)
                << '\n';

            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Valid transaction accepted\n";

        Transaction duplicate = valid;
        duplicate.inputs.push_back(
            duplicate.inputs[0]);

        if (!expect_rejection(
                duplicate,
                utxos,
                MempoolRejectReason::DuplicateInput,
                "Duplicate input rejected")) {

            return EXIT_FAILURE;
        }

        Transaction missing = valid;
        missing.inputs[0].previous_txid =
            sha256("nonexistent transaction");

        if (!expect_rejection(
                missing,
                utxos,
                MempoolRejectReason::MissingInput,
                "Missing input rejected")) {

            return EXIT_FAILURE;
        }

        Transaction too_large =
            make_transaction(
                funding_txid,
                5000001,
                owner.address());

        if (!expect_rejection(
                too_large,
                utxos,
                MempoolRejectReason::InsufficientInputValue,
                "Insufficient input value rejected")) {

            return EXIT_FAILURE;
        }

        Transaction zero_output =
            make_transaction(
                funding_txid,
                0,
                owner.address());

        if (!expect_rejection(
                zero_output,
                utxos,
                MempoolRejectReason::InvalidTransaction,
                "Zero-value output rejected")) {

            return EXIT_FAILURE;
        }

        Transaction empty_recipient =
            make_transaction(
                funding_txid,
                1000000,
                "");

        if (!expect_rejection(
                empty_recipient,
                utxos,
                MempoolRejectReason::InvalidTransaction,
                "Empty recipient rejected")) {

            return EXIT_FAILURE;
        }

        Transaction invalid_version =
            valid;

        invalid_version.version = 0;

        if (!expect_rejection(
                invalid_version,
                utxos,
                MempoolRejectReason::InvalidTransaction,
                "Invalid version rejected")) {

            return EXIT_FAILURE;
        }

        std::cout
            << "ALL MEMPOOL VALIDATION TESTS PASSED\n";

        return EXIT_SUCCESS;

    } catch (const std::exception& e) {
        std::cerr
            << "[FAIL] "
            << e.what()
            << '\n';

        return EXIT_FAILURE;
    }
}
