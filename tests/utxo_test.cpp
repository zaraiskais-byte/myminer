#include <cstdlib>
#include <iostream>

#include <caesar/utxo.hpp>

using namespace caesar;

static TransactionOutput make_output(
    std::uint64_t amount,
    const std::string& recipient) {

    TransactionOutput output;
    output.amount = amount;
    output.recipient = recipient;
    return output;
}

int main() {
    std::cout << "=== Caesar CZR UTXO Tests ===\n";

    UTXOSet utxos;

    const Hash256 funding_txid =
        sha256("funding transaction");

    const OutPoint funded{
        funding_txid,
        0
    };

    const TransactionOutput funded_output =
        make_output(5000000, "CZ1ALICE");

    if (!utxos.add(funded, funded_output)) {
        std::cerr << "[FAIL] Add UTXO\n";
        return EXIT_FAILURE;
    }
    std::cout << "[PASS] Add UTXO\n";

    if (!utxos.contains(funded)) {
        std::cerr << "[FAIL] UTXO lookup\n";
        return EXIT_FAILURE;
    }
    std::cout << "[PASS] UTXO lookup\n";

    const TransactionOutput* stored =
        utxos.get(funded);

    if (!stored || stored->amount != 5000000) {
        std::cerr << "[FAIL] UTXO value\n";
        return EXIT_FAILURE;
    }
    std::cout << "[PASS] UTXO value\n";

    TransactionInput input;
    input.previous_txid = funding_txid;
    input.output_index = 0;

    Transaction spend;
    spend.inputs.push_back(input);
    spend.outputs.push_back(
        make_output(4000000, "CZ1BOB"));

    if (!validate_transaction_against_utxo(
            spend, utxos)) {

        std::cerr << "[FAIL] Valid UTXO spend rejected\n";
        return EXIT_FAILURE;
    }
    std::cout << "[PASS] Valid UTXO spend\n";

    if (!utxos.spend(funded)) {
        std::cerr << "[FAIL] Spend UTXO\n";
        return EXIT_FAILURE;
    }
    std::cout << "[PASS] UTXO consumed\n";

    if (utxos.contains(funded)) {
        std::cerr << "[FAIL] Spent UTXO still exists\n";
        return EXIT_FAILURE;
    }
    std::cout << "[PASS] Spent UTXO removed\n";

    if (validate_transaction_against_utxo(
            spend, utxos)) {

        std::cerr << "[FAIL] Double spend accepted\n";
        return EXIT_FAILURE;
    }
    std::cout << "[PASS] Double spend rejected\n";

    UTXOSet duplicate_utxos;
    duplicate_utxos.add(funded, funded_output);

    Transaction duplicate_input;
    duplicate_input.inputs.push_back(input);
    duplicate_input.inputs.push_back(input);

    duplicate_input.outputs.push_back(
        make_output(4000000, "CZ1BOB"));

    if (validate_transaction_against_utxo(
            duplicate_input, duplicate_utxos)) {

        std::cerr << "[FAIL] Duplicate input accepted\n";
        return EXIT_FAILURE;
    }
    std::cout << "[PASS] Duplicate input rejected\n";

    std::cout << "ALL UTXO TESTS PASSED\n";

    return EXIT_SUCCESS;
}
