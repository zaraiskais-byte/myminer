#include <cstdlib>
#include <iostream>
#include <string>
#include <stdexcept>
#include <vector>

#include <caesar/block.hpp>

using namespace caesar;

static Transaction make_transaction(
    const std::string& seed,
    std::uint64_t amount) {

    Transaction tx;

    TransactionInput input;
    input.previous_txid = sha256(seed);
    input.output_index = 0;

    tx.inputs.push_back(input);

    TransactionOutput output;
    output.amount = amount;
    output.recipient = "CZ1-block-test";

    tx.outputs.push_back(output);

    return tx;
}

static Block make_genesis() {
    Block block;

    block.header.version = 1;
    block.header.height = 0;
    block.header.timestamp = 1000;
    block.header.nonce = 0;
    block.header.difficulty = 1;

    block.transactions.push_back(
        make_transaction(
            "genesis transaction",
            1000));

    block.update_merkle_root();

    std::uint64_t found_nonce = 0;
    Hash256 found_hash{};

    if (!mine_pow(
            block.pow_header(),
            block.header.difficulty,
            0,
            1000000,
            found_nonce,
            found_hash)) {
        throw std::runtime_error(
            "failed to mine test genesis block");
    }

    block.header.nonce = found_nonce;

    return block;
}

int main() {
    std::cout
        << "=== Caesar CZR Block Tests ===\n";

    try {
        Block genesis = make_genesis();

        if (!genesis.validate_basic()) {
            std::cerr
                << "[FAIL] Valid genesis block rejected\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Valid genesis block accepted\n";

        const Hash256 genesis_hash =
            genesis.hash();

        if (genesis_hash == Hash256{}) {
            std::cerr
                << "[FAIL] Empty genesis hash\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Genesis block hash generated\n";

        Block block1;

        block1.header.version = 1;
        block1.header.height = 1;
        block1.header.previous_hash =
            genesis_hash;
        block1.header.timestamp = 1060;
        block1.header.nonce = 42;
        block1.header.difficulty = 1;

        block1.transactions.push_back(
            make_coinbase_transaction(1, "CZ1-miner"));

        block1.transactions.push_back(
            make_transaction(
                "block one transaction",
                2000));

        block1.transactions.push_back(
            make_transaction(
                "block one second transaction",
                3000));

        block1.update_merkle_root();

        if (!block1.validate_basic()) {
            std::cerr
                << "[FAIL] Valid block 1 rejected\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Block 1 accepted\n";

        if (!validate_block_link(
                genesis,
                block1)) {

            std::cerr
                << "[FAIL] Block linkage rejected\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Block linked to previous block\n";

        std::vector<Block> chain{
            genesis,
            block1
        };

        if (!validate_block_chain(chain)) {
            std::cerr
                << "[FAIL] Valid block chain rejected\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Block chain validation succeeded\n";

        const Hash256 root =
            block1.header.merkle_root;

        if (root != block1.calculate_merkle_root()) {
            std::cerr
                << "[FAIL] Merkle root mismatch\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Merkle root verified\n";

        Block changed_tx = block1;

        changed_tx.transactions[0]
            .outputs[0]
            .amount++;

        if (changed_tx.validate_merkle_root()) {
            std::cerr
                << "[FAIL] Modified transaction kept old Merkle root\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Modified transaction invalidates Merkle root\n";

        Block repaired_tx = changed_tx;
        repaired_tx.update_merkle_root();

        if (!repaired_tx.validate_merkle_root()) {
            std::cerr
                << "[FAIL] Merkle root could not be rebuilt\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Merkle root rebuilt after transaction change\n";

        Block wrong_previous = block1;

        wrong_previous.header.previous_hash =
            sha256("wrong previous block");

        if (validate_block_link(
                genesis,
                wrong_previous)) {

            std::cerr
                << "[FAIL] Wrong previous hash accepted\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Wrong previous hash rejected\n";

        Block wrong_height = block1;
        wrong_height.header.height = 7;

        if (validate_block_link(
                genesis,
                wrong_height)) {

            std::cerr
                << "[FAIL] Wrong block height accepted\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Wrong block height rejected\n";

        Block tampered_header = block1;
        tampered_header.header.nonce++;

        if (tampered_header.hash() ==
            block1.hash()) {

            std::cerr
                << "[FAIL] Header mutation kept same hash\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Header mutation changes block hash\n";

        const auto binary =
            block1.serialize_binary();

        if (binary.empty()) {
            std::cerr
                << "[FAIL] Empty block serialization\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Binary block serialization generated\n";

        std::cout
            << "Genesis hash: "
            << hash_to_hex(genesis_hash)
            << '\n';

        std::cout
            << "Block 1 hash: "
            << hash_to_hex(block1.hash())
            << '\n';

        std::cout
            << "Merkle root: "
            << hash_to_hex(block1.header.merkle_root)
            << '\n';

        std::cout
            << "ALL BLOCK TESTS PASSED\n";

        return EXIT_SUCCESS;

    } catch (const std::exception& e) {
        std::cerr
            << "[FAIL] "
            << e.what()
            << '\n';

        return EXIT_FAILURE;
    }
}
