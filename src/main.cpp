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
        Block genesis{
            0,
            "0",
            "Caesar CZR Genesis",
            0
        };

        Block block1{
            1,
            calculate_hash(genesis),
            "Caesar CZR Block 1",
            0
        };

        std::vector<Block> chain{genesis, block1};

        std::cout << "=== Caesar CZR Cryptographic Foundation ===\n";
        std::cout << "Hash algorithm: SHA-256\n";
        std::cout << "Genesis hash: " << calculate_hash(genesis) << '\n';
        std::cout << "Block 1 hash: " << calculate_hash(block1) << '\n';
        std::cout << "Chain blocks: " << chain.size() << '\n';
        std::cout << "Chain validation: "
                  << (validate_chain(chain) ? "PASS" : "FAIL")
                  << '\n';

        return validate_chain(chain) ? 0 : 1;
    }
    catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 1;
    }
}
