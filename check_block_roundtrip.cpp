#include <iostream>
#include <filesystem>

#include <caesar/node.hpp>
#include <caesar/block_builder.hpp>

using namespace caesar;

static std::string hex_hash(const Hash256& h) {
    return hash_to_hex(h);
}

int main() {
    const auto dir =
        std::filesystem::path("check_roundtrip_data");

    std::filesystem::remove_all(dir);

    CaesarNode node(dir.string(), 19644, 1);
    node.start();

    node.mine_one_block("CAESAR_ROUNDTRIP_TEST", 1000000);

    const auto chain = node.chain();

    if (chain.size() < 2) {
        std::cerr << "ERROR: chain size < 2\n";
        node.stop();
        return 1;
    }

    const Block& original = chain[1];

    const auto encoded = original.serialize_full_binary();
    const Block decoded = Block::deserialize_full(encoded);

    std::cout << "original_height=" << original.header.height << '\n';
    std::cout << "decoded_height=" << decoded.header.height << '\n';

    std::cout << "original_hash="
              << hex_hash(original.hash()) << '\n';

    std::cout << "decoded_hash="
              << hex_hash(decoded.hash()) << '\n';

    std::cout << "original_prev="
              << hex_hash(original.header.previous_hash) << '\n';

    std::cout << "decoded_prev="
              << hex_hash(decoded.header.previous_hash) << '\n';

    std::cout << "original_merkle="
              << hex_hash(original.header.merkle_root) << '\n';

    std::cout << "decoded_merkle="
              << hex_hash(decoded.header.merkle_root) << '\n';

    std::cout << "original_witness="
              << hex_hash(original.header.witness_root) << '\n';

    std::cout << "decoded_witness="
              << hex_hash(decoded.header.witness_root) << '\n';

    std::cout << "original_tx_count="
              << original.transactions.size() << '\n';

    std::cout << "decoded_tx_count="
              << decoded.transactions.size() << '\n';

    std::cout << "original_basic="
              << original.validate_basic() << '\n';

    std::cout << "decoded_basic="
              << decoded.validate_basic() << '\n';

    std::cout << "original_pow="
              << original.validate_pow() << '\n';

    std::cout << "decoded_pow="
              << decoded.validate_pow() << '\n';

    std::cout << "original_merkle_valid="
              << original.validate_merkle_root() << '\n';

    std::cout << "decoded_merkle_valid="
              << decoded.validate_merkle_root() << '\n';

    std::cout << "original_witness_valid="
              << original.validate_witness_root() << '\n';

    std::cout << "decoded_witness_valid="
              << decoded.validate_witness_root() << '\n';

    bool identical =
        original.hash() == decoded.hash() &&
        original.header.previous_hash ==
            decoded.header.previous_hash &&
        original.header.merkle_root ==
            decoded.header.merkle_root &&
        original.header.witness_root ==
            decoded.header.witness_root &&
        original.transactions.size() ==
            decoded.transactions.size();

    std::cout << "ROUNDTRIP_IDENTICAL="
              << identical << '\n';

    node.stop();
    std::filesystem::remove_all(dir);

    return identical ? 0 : 2;
}
