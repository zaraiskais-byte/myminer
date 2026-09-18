#include <cstdint>
#include <caesar/blockchain_storage.hpp>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <openssl/evp.h>

struct Block {
    std::uint64_t height{};
    std::string previous_hash;
    std::string data;
    std::uint64_t nonce{};
};

std::string block_header(const Block& block) {
    std::ostringstream out;
    out << block.height << '|'
        << block.previous_hash << '|'
        << block.data << '|'
        << block.nonce;
    return out.str();
}

std::string sha256(const std::string& input) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx)
        throw std::runtime_error("EVP_MD_CTX_new failed");

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_size = 0;

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx, input.data(), input.size()) != 1 ||
        EVP_DigestFinal_ex(ctx, digest, &digest_size) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("SHA-256 calculation failed");
    }

    EVP_MD_CTX_free(ctx);

    std::ostringstream out;
    out << std::hex << std::setfill('0');

    for (unsigned int i = 0; i < digest_size; ++i)
        out << std::setw(2) << static_cast<unsigned int>(digest[i]);

    return out.str();
}

std::string calculate_hash(const Block& block) {
    return sha256(block_header(block));
}

bool validate_chain(const std::vector<Block>& chain) {
    if (chain.empty())
        return false;

    if (chain[0].height != 0 || chain[0].previous_hash != "0")
        return false;

    for (std::size_t i = 1; i < chain.size(); ++i) {
        const Block& current = chain[i];
        const Block& previous = chain[i - 1];

        if (current.height != previous.height + 1)
            return false;

        if (current.previous_hash != calculate_hash(previous))
            return false;
    }

    return true;
}

int main() {
    try {
        std::cout << "=== Caesar CZR Node ===\n";

        const std::filesystem::path data_dir =
            std::filesystem::path("data");

        std::filesystem::create_directories(data_dir);

        const std::filesystem::path chain_file =
            data_dir / "blockchain.dat";

        caesar::BlockchainStorage storage(chain_file);

        if (!storage.exists()) {
            std::cout << "Blockchain storage: NEW\n";

            caesar::Block genesis;
            genesis.header.version = 1;
            genesis.header.height = 0;
            genesis.header.previous_hash = {};
            genesis.header.timestamp = 0;
            genesis.header.nonce = 0;
            genesis.header.difficulty = 0;

            caesar::Transaction genesis_tx;
            genesis_tx.outputs.push_back(
                caesar::TransactionOutput{
                    1,
                    "CAESAR_GENESIS_BURN"
                });

            genesis.transactions.push_back(genesis_tx);
            genesis.update_merkle_root();

            storage.save({genesis});

            std::cout << "Genesis created and saved.\n";
        }

        auto chain = storage.load();

        std::cout << "Blockchain blocks: "
                  << chain.size() << "\n";

        std::cout << "Chain validation: "
                  << (caesar::validate_block_chain(chain)
                          ? "PASS"
                          : "FAIL")
                  << "\n";

        std::cout << "Storage file: "
                  << storage.path()
                  << "\n";

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Caesar node error: "
                  << e.what() << "\n";
        return 1;
    }
}
