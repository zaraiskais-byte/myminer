#include <caesar/block_builder.hpp>
#include <caesar/node.hpp>

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}

} // namespace

int main() {
    const auto base =
        std::filesystem::temp_directory_path() /
        "caesar_node_chain_replacement_test";

    std::error_code ec;
    std::filesystem::remove_all(base, ec);

    try {
        caesar::CaesarNode node(base, 19447, 1);
        node.start();

        const auto before = node.chain();

        require(before.size() == 1,
                "fresh node must contain only genesis");

        // Build one valid successor using the same consensus rules as
        // normal node mining. This creates a candidate with strictly
        // more chainwork than the genesis-only chain.
        caesar::Mempool empty_mempool;

        caesar::Block candidate =
            caesar::BlockBuilder::build(
                before.back(),
                empty_mempool,
                "node-replacement-test-miner",
                0,
                caesar::CZR_INITIAL_MINING_DIFFICULTY);

        require(
            caesar::BlockBuilder::mine(
                candidate,
                0,
                1000000),
            "failed to mine replacement candidate");

        require(
            caesar::validate_block_consensus(
                candidate,
                before,
                caesar::rebuild_utxo_set(before)),
            "replacement candidate failed consensus");

        auto replacement = before;
        replacement.push_back(candidate);

        require(
            node.replace_chain(replacement),
            "valid higher-work chain must replace canonical chain");

        const auto after = node.chain();

        require(after.size() == 2,
                "successful replacement must persist two blocks");

        require(
            after.back().hash() == candidate.hash(),
            "successful replacement persisted wrong tip");

        // Equal-work/current chain must not replace anything.
        require(
            !node.replace_chain(after),
            "equal-work chain must not replace canonical chain");

        const auto after_equal = node.chain();

        require(
            after_equal.size() == after.size(),
            "equal-work rejection changed chain size");

        // Invalid candidate must be rejected without changing storage.
        auto invalid = after;
        invalid[1].header.version = 0;

        require(
            !node.replace_chain(invalid),
            "invalid candidate must be rejected");

        const auto after_invalid = node.chain();

        require(
            after_invalid.size() == after.size(),
            "invalid rejection changed chain size");

        require(
            after_invalid.back().hash() == after.back().hash(),
            "invalid rejection changed canonical tip");

        node.stop();

        std::filesystem::remove_all(base, ec);

        std::cout << "Node chain replacement integration test passed\n";
        return 0;
    } catch (...) {
        std::filesystem::remove_all(base, ec);
        throw;
    }
}
