#include <cstdlib>
#include <iostream>
#include <string>

#include <caesar/transaction.hpp>

using namespace caesar;

static Transaction make_transaction() {
    Transaction tx;

    TransactionInput input;
    input.previous_txid =
        sha256("previous transaction");
    input.output_index = 2;

    tx.inputs.push_back(input);

    TransactionOutput output;
    output.amount = 123456;
    output.recipient = "CZ1-test-recipient";

    tx.outputs.push_back(output);

    return tx;
}

int main() {
    std::cout << "=== Caesar CZR Transaction Tests ===\n";

    try {
        Transaction tx = make_transaction();

        if (!tx.validate()) {
            std::cerr << "[FAIL] Valid transaction rejected\n";
            return EXIT_FAILURE;
        }

        std::cout << "[PASS] Valid transaction accepted\n";

        Transaction reserved_index = tx;
        reserved_index.inputs[0].output_index =
            UINT32_MAX;

        if (reserved_index.validate()) {
            std::cerr
                << "[FAIL] Normal transaction accepted reserved coinbase index\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Coinbase input index reserved\n";

        const auto binary =
            tx.serialize_binary();

        if (binary.empty()) {
            std::cerr << "[FAIL] Empty binary serialization\n";
            return EXIT_FAILURE;
        }

        std::cout << "[PASS] Binary serialization generated\n";

        const Hash256 original_id =
            tx.txid();

        Transaction restored =
            Transaction::deserialize(binary);

        if (!restored.validate()) {
            std::cerr << "[FAIL] Deserialized transaction invalid\n";
            return EXIT_FAILURE;
        }

        std::cout << "[PASS] Binary transaction deserialized\n";

        if (restored.serialize_binary() != binary) {
            std::cerr << "[FAIL] Serialization round trip changed bytes\n";
            return EXIT_FAILURE;
        }

        std::cout << "[PASS] Serialization round trip preserved bytes\n";

        if (restored.txid() != original_id) {
            std::cerr << "[FAIL] Deserialized TXID mismatch\n";
            return EXIT_FAILURE;
        }

        std::cout << "[PASS] Deserialized TXID matches original\n";

        std::vector<std::uint8_t> truncated =
            binary;

        truncated.pop_back();

        try {
            (void)Transaction::deserialize(truncated);

            std::cerr << "[FAIL] Truncated transaction accepted\n";
            return EXIT_FAILURE;

        } catch (const std::exception&) {
            std::cout << "[PASS] Truncated transaction rejected\n";
        }

        std::vector<std::uint8_t> trailing =
            binary;

        trailing.push_back(0xCA);

        try {
            (void)Transaction::deserialize(trailing);

            std::cerr << "[FAIL] Trailing bytes accepted\n";
            return EXIT_FAILURE;

        } catch (const std::exception&) {
            std::cout << "[PASS] Trailing bytes rejected\n";
        }

        Transaction changed = restored;
        changed.outputs[0].amount++;

        if (changed.txid() == original_id) {
            std::cerr << "[FAIL] Changed transaction kept same TXID\n";
            return EXIT_FAILURE;
        }

        std::cout << "[PASS] Transaction mutation changes TXID\n";

        Transaction witness_tx = restored;

        TransactionWitness witness;
        witness.public_key = "test-public-key";
        witness.signature = {0x01, 0x02, 0x03, 0x04};

        witness_tx.witness.inputs.push_back(witness);

        const Hash256 witness_txid = witness_tx.txid();
        const Hash256 witness_wtxid = witness_tx.wtxid();

        witness_tx.witness.inputs[0].signature[0] ^= 0x01;

        if (witness_tx.txid() != witness_txid) {
            std::cerr << "[FAIL] Witness mutation changed TXID\n";
            return EXIT_FAILURE;
        }

        std::cout << "[PASS] Witness mutation keeps TXID unchanged\n";

        if (witness_tx.wtxid() == witness_wtxid) {
            std::cerr << "[FAIL] Witness mutation kept same WTXID\n";
            return EXIT_FAILURE;
        }

        std::cout << "[PASS] Witness mutation changes WTXID\n";

        std::cout << "TXID: "
                  << hash_to_hex(original_id) << '\n';

        std::cout << "ALL TRANSACTION TESTS PASSED\n";
        return EXIT_SUCCESS;

    } catch (const std::exception& e) {
        std::cerr << "[FAIL] " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
