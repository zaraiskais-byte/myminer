#include <cstdint>
#include <iostream>
#include <vector>

#include <caesar/block_builder.hpp>
#include <caesar/wallet.hpp>
#include <caesar/ownership.hpp>
#include <caesar/transaction_signature.hpp>

using namespace caesar;

bool check(const char* name, bool condition) {
    std::cout << name << " | "
              << (condition ? "PASS" : "FAIL")
              << '\n';
    return condition;
}

int main() {
    bool ok = true;

    std::vector<Block> chain;

    for (std::uint64_t i = 0;
         i < CZR_DIFFICULTY_WINDOW + 1;
         ++i) {

        Block block;
        block.header.version = 1;
        block.header.height = i;
        block.header.timestamp = i * 120;
        block.header.difficulty = 0;

        if (i > 0)
            block.header.previous_hash = chain.back().hash();

        chain.push_back(block);
    }

    Mempool mempool;

    Block candidate =
        BlockBuilder::build(
            chain.back(),
            mempool,
            "CZ1_TEST_MINER",
            chain.back().header.timestamp + 120,
            0);

    ok &= check(
        "Candidate PoW valid",
        BlockBuilder::mine(candidate, 0, 1));

    UTXOSet utxos;

    ok &= check(
        "Valid full consensus block accepted",
        validate_block_consensus(candidate, chain, utxos));

    Wallet alice;
    Wallet recipient;

    const Hash256 signed_funding_txid =
        sha256("full consensus signed funding");

    UTXOSet signed_utxos;
    signed_utxos.add(
        OutPoint{signed_funding_txid, 0},
        TransactionOutput{100, alice.address()});

    Transaction signed_spend;
    signed_spend.inputs.push_back(
        TransactionInput{signed_funding_txid, 0});
    signed_spend.outputs.push_back(
        TransactionOutput{90, recipient.address()});

    const auto signed_signature =
        sign_transaction_input(
            signed_spend,
            0,
            alice.private_key());

    signed_spend.witness.inputs.push_back(
        TransactionWitness{
            alice.public_key(),
            signed_signature});

    ok &= check(
        "Signed witness validates",
        validate_transaction_witness(
            signed_spend,
            signed_utxos));

    Block witness_block = candidate;
    witness_block.transactions.push_back(signed_spend);
    witness_block.update_merkle_root();

    const auto block_full_serialized =
        witness_block.serialize_full_binary();

    ok &= check(
        "Block full serialization includes witness",
        block_full_serialized.size() > witness_block.serialize_binary().size());

    const Block witness_block_roundtrip =
        Block::deserialize_full(block_full_serialized);

    ok &= check(
        "Block survives full serialization",
        witness_block_roundtrip.header.version ==
            witness_block.header.version &&
        witness_block_roundtrip.header.height ==
            witness_block.header.height &&
        witness_block_roundtrip.header.previous_hash ==
            witness_block.header.previous_hash &&
        witness_block_roundtrip.header.merkle_root ==
            witness_block.header.merkle_root &&
        witness_block_roundtrip.header.timestamp ==
            witness_block.header.timestamp &&
        witness_block_roundtrip.header.nonce ==
            witness_block.header.nonce &&
        witness_block_roundtrip.header.difficulty ==
            witness_block.header.difficulty &&
        witness_block_roundtrip.transactions.size() ==
            witness_block.transactions.size());

    ok &= check(
        "Block witness survives round-trip",
        witness_block_roundtrip.transactions.size() == 2 &&
        witness_block_roundtrip.transactions[1].witness.inputs.size() == 1 &&
        witness_block_roundtrip.transactions[1].witness.inputs[0].public_key ==
            alice.public_key() &&
        witness_block_roundtrip.transactions[1].witness.inputs[0].signature ==
            signed_signature);

    ok &= check(
        "Round-trip block merkle root remains valid",
        witness_block_roundtrip.validate_merkle_root());

    Block mutated_witness_block = witness_block_roundtrip;

    const Hash256 original_txid =
        mutated_witness_block.transactions[1].txid();

    const Hash256 original_merkle_root =
        mutated_witness_block.header.merkle_root;

    const Hash256 original_witness_root =
        mutated_witness_block.header.witness_root;

    if (!mutated_witness_block.transactions[1]
             .witness.inputs[0].signature.empty()) {
        mutated_witness_block.transactions[1]
            .witness.inputs[0].signature[0] ^= 0x01;
    }

    ok &= check(
        "Witness mutation keeps TXID unchanged",
        mutated_witness_block.transactions[1].txid() ==
            original_txid);

    ok &= check(
        "Witness mutation keeps merkle root unchanged",
        mutated_witness_block.calculate_merkle_root() ==
            original_merkle_root);

    ok &= check(
        "Witness mutation changes witness root",
        mutated_witness_block.calculate_witness_root() !=
            original_witness_root);

    ok &= check(
        "Mutated witness block rejected",
        !mutated_witness_block.validate_witness_root());

    Transaction mutated_signed_spend = signed_spend;

    if (!mutated_signed_spend.witness.inputs.empty() &&
        !mutated_signed_spend.witness.inputs[0].signature.empty()) {
        mutated_signed_spend.witness.inputs[0].signature[0] ^= 0x01;
    }

    ok &= check(
        "Mutated witness signature rejected",
        !validate_transaction_witness(
            mutated_signed_spend,
            signed_utxos));

    const Hash256 signed_txid_before = signed_spend.txid();
    const auto full_serialized = signed_spend.serialize_full_binary();
    const Transaction signed_roundtrip =
        Transaction::deserialize_full(full_serialized);

    ok &= check(
        "Witness survives full serialization",
        signed_roundtrip.witness.inputs.size() == 1 &&
        signed_roundtrip.witness.inputs[0].public_key == alice.public_key() &&
        signed_roundtrip.witness.inputs[0].signature == signed_signature);

    ok &= check(
        "Witness remains valid after deserialization",
        validate_transaction_witness(
            signed_roundtrip,
            signed_utxos));

    ok &= check(
        "TXID excludes witness",
        signed_roundtrip.txid() == signed_txid_before);

    // Security regression test:
    // An unsigned transaction must NOT be accepted by block consensus.
    Transaction funding;
    funding.outputs.push_back(
        TransactionOutput{100, "CZ1_OWNER"});

    const Hash256 funding_txid = funding.txid();

    UTXOSet funded_utxos;
    funded_utxos.add(
        OutPoint{funding_txid, 0},
        funding.outputs[0]);

    Transaction unsigned_spend;
    unsigned_spend.inputs.push_back(
        TransactionInput{funding_txid, 0});
    unsigned_spend.outputs.push_back(
        TransactionOutput{90, "CZ1_RECIPIENT"});

    Block unsigned_block = candidate;
    unsigned_block.transactions.push_back(unsigned_spend);
    unsigned_block.update_merkle_root();

    ok &= check(
        "Unsigned spend rejected by full consensus",
        !validate_block_consensus(
            unsigned_block,
            chain,
            funded_utxos));

    Transaction missing_utxo_spend;
    missing_utxo_spend.inputs.push_back(
        TransactionInput{
            sha256("nonexistent-utxo"),
            0});
    missing_utxo_spend.outputs.push_back(
        TransactionOutput{90, recipient.address()});

    Block missing_utxo_block = candidate;
    missing_utxo_block.transactions.push_back(
        missing_utxo_spend);
    missing_utxo_block.update_merkle_root();

    ok &= check(
        "Missing UTXO rejected by full consensus",
        !validate_block_consensus(
            missing_utxo_block,
            chain,
            funded_utxos));

    Transaction double_spend;
    double_spend.inputs.push_back(
        TransactionInput{funding_txid, 0});
    double_spend.outputs.push_back(
        TransactionOutput{80, "CZ1_DOUBLE_SPEND"});

    const auto double_spend_signature =
        sign_transaction_input(
            double_spend,
            0,
            alice.private_key());

    double_spend.witness.inputs.push_back(
        TransactionWitness{
            alice.public_key(),
            double_spend_signature});

    Block double_spend_block = candidate;
    double_spend_block.transactions.push_back(
        signed_spend);
    double_spend_block.transactions.push_back(
        double_spend);
    double_spend_block.update_merkle_root();

    ok &= check(
        "Double-spend rejected by full consensus",
        !validate_block_consensus(
            double_spend_block,
            chain,
            funded_utxos));

    Block bad_height = candidate;
    bad_height.header.height += 1;

    ok &= check(
        "Wrong height rejected",
        !bad_height.validate_against_chain(chain));

    Block bad_previous = candidate;
    bad_previous.header.previous_hash[0] ^= 1;

    ok &= check(
        "Wrong previous hash rejected",
        !bad_previous.validate_against_chain(chain));

    Block bad_time = candidate;
    bad_time.header.timestamp =
        chain.back().header.timestamp - 1;

    ok &= check(
        "Old timestamp rejected",
        !bad_time.validate_against_chain(chain));

    // Security regression test:
    // The cumulative coinbase issuance must never exceed
    // the 21,000,000 CZR maximum supply.
    {
        Block valid_issuance_block = candidate;

        const std::uint64_t allowed_amount =
            CZR_MAX_SUPPLY;

        valid_issuance_block.transactions[0].outputs[0].amount =
            block_subsidy(valid_issuance_block.header.height);

        valid_issuance_block.update_merkle_root();

        ok &= check(
            "Supply cap boundary accepted by issuance rule",
            validate_total_coinbase_issuance(
                valid_issuance_block,
                {}));

        Block excess_issuance_block = candidate;

        excess_issuance_block.transactions[0].outputs[0].amount =
            allowed_amount + 1;

        excess_issuance_block.update_merkle_root();

        ok &= check(
            "Supply cap exceeded rejected by issuance rule",
            !validate_total_coinbase_issuance(
                excess_issuance_block,
                {}));
    }

    std::cout << "Overall: "
              << (ok ? "PASS" : "FAIL")
              << '\n';

    return ok ? 0 : 1;
}
