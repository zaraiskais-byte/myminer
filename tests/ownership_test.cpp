#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <caesar/ownership.hpp>
#include <caesar/wallet.hpp>

using namespace caesar;

int main() {
    std::cout << "=== Caesar CZR Ownership Tests ===\n";

    try {
        Wallet alice;
        Wallet attacker;

        UTXOSet utxos;

        const Hash256 funding_txid =
            sha256("Alice ownership funding");

        const OutPoint funded{
            funding_txid,
            0
        };

        TransactionOutput funding_output;
        funding_output.amount = 5000000;
        funding_output.recipient = alice.address();

        if (!utxos.add(funded, funding_output)) {
            std::cerr << "[FAIL] Funding UTXO\n";
            return EXIT_FAILURE;
        }

        Transaction tx;

        TransactionInput input;
        input.previous_txid = funding_txid;
        input.output_index = 0;

        tx.inputs.push_back(input);

        TransactionOutput destination;
        destination.amount = 4000000;
        destination.recipient = attacker.address();

        tx.outputs.push_back(destination);

        const auto alice_signature =
            sign_transaction_input(
                tx,
                0,
                alice.private_key());

        std::vector<EVP_PKEY*> alice_handles{
            alice.public_key_handle()
        };

        std::vector<std::string> alice_public_keys{
            alice.public_key()
        };

        std::vector<std::vector<unsigned char>> signatures{
            alice_signature
        };

        if (!validate_signed_transaction_ownership(
                tx,
                utxos,
                alice_handles,
                alice_public_keys,
                signatures)) {

            std::cerr << "[FAIL] Valid owner transaction rejected\n";
            return EXIT_FAILURE;
        }

        std::cout << "[PASS] Correct owner + signature accepted\n";

        std::vector<EVP_PKEY*> attacker_handles{
            attacker.public_key_handle()
        };

        std::vector<std::string> attacker_public_keys{
            attacker.public_key()
        };

        if (validate_signed_transaction_ownership(
                tx,
                utxos,
                attacker_handles,
                attacker_public_keys,
                signatures)) {

            std::cerr << "[FAIL] Wrong owner accepted\n";
            return EXIT_FAILURE;
        }

        std::cout << "[PASS] Wrong owner rejected\n";

        std::vector<EVP_PKEY*> alice_wrong_handle{
            attacker.public_key_handle()
        };

        if (validate_signed_transaction_ownership(
                tx,
                utxos,
                alice_wrong_handle,
                alice_public_keys,
                signatures)) {

            std::cerr << "[FAIL] Wrong signing key accepted\n";
            return EXIT_FAILURE;
        }

        std::cout << "[PASS] Wrong signing key rejected\n";

        Transaction modified = tx;
        modified.outputs[0].amount = 4500000;

        if (validate_signed_transaction_ownership(
                modified,
                utxos,
                alice_handles,
                alice_public_keys,
                signatures)) {

            std::cerr << "[FAIL] Modified transaction accepted\n";
            return EXIT_FAILURE;
        }

        std::cout << "[PASS] Modified transaction rejected\n";

        std::cout << "Owner address: "
                  << alice.address() << '\n';

        std::cout << "ALL OWNERSHIP TESTS PASSED\n";
        return EXIT_SUCCESS;

    } catch (const std::exception& e) {
        std::cerr << "[FAIL] " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
