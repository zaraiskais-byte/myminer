#include <cstdlib>
#include <iostream>

#include <caesar/coinbase.hpp>
#include <caesar/economics.hpp>

using namespace caesar;

int main() {
    try {
        std::cout
            << "=== Caesar CZR Coinbase Tests ===\n";

        if (block_subsidy(0) != 0) {
            std::cerr
                << "[FAIL] Genesis subsidy must be zero\n";
            return EXIT_FAILURE;
        }

        if (block_subsidy(1) !=
            CZR_INITIAL_SUBSIDY) {

            std::cerr
                << "[FAIL] Initial subsidy incorrect\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Initial subsidy verified\n";

        const EconomicsPolicy economics_policy =
            czr_economics_policy();

        if (economics_policy.max_supply !=
                CZR_MAX_SUPPLY ||
            economics_policy.initial_reward !=
                CZR_INITIAL_SUBSIDY ||
            economics_policy.halving_interval !=
                CZR_HALVING_INTERVAL) {

            std::cerr
                << "[FAIL] Coinbase constants diverge from economics policy\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Coinbase constants match economics policy\n";

        if (block_subsidy(1) !=
            economics_block_reward(
                economics_policy,
                1)) {

            std::cerr
                << "[FAIL] Coinbase reward diverges from economics reward\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Coinbase reward is economics-derived\n";

        const auto first_halving =
            block_subsidy(
                CZR_HALVING_INTERVAL);

        if (first_halving !=
            CZR_INITIAL_SUBSIDY / 2) {

            std::cerr
                << "[FAIL] First halving incorrect\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Halving rule verified\n";

        const std::uint64_t second_halving_height =
            CZR_HALVING_INTERVAL * 2;

        if (block_subsidy(second_halving_height) !=
            economics_block_reward(
                economics_policy,
                second_halving_height)) {

            std::cerr
                << "[FAIL] Second-halving reward diverges from economics\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Second-halving reward matches economics\n";

        Transaction coinbase =
            make_coinbase_transaction(
                1,
                "CZ1-miner");

        if (!is_coinbase_transaction(
                coinbase)) {

            std::cerr
                << "[FAIL] Coinbase not recognized\n";
            return EXIT_FAILURE;
        }

        if (!validate_coinbase_transaction(
                coinbase,
                1)) {

            std::cerr
                << "[FAIL] Valid coinbase rejected\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Valid coinbase accepted\n";

        Transaction next_height =
            make_coinbase_transaction(
                2,
                "CZ1-miner");

        if (coinbase.txid() == next_height.txid()) {
            std::cerr
                << "[FAIL] Coinbase TXID reused across heights\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Coinbase TXID is unique across heights\n";

        Transaction altered_marker =
            make_coinbase_transaction(
                1,
                "CZ1-miner",
                1);

        if (altered_marker.txid() == coinbase.txid()) {
            std::cerr
                << "[FAIL] Coinbase extra nonce did not change TXID\n";
            return EXIT_FAILURE;
        }

        if (validate_coinbase_transaction(
                altered_marker,
                1)) {

            std::cerr
                << "[FAIL] Non-default coinbase commitment accepted\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Coinbase extra nonce commitment verified\n";

        Transaction oversized =
            coinbase;

        oversized.outputs[0].amount++;

        if (validate_coinbase_transaction(
                oversized,
                1)) {

            std::cerr
                << "[FAIL] Oversized coinbase accepted\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Oversized coinbase rejected\n";

        Transaction normal =
            coinbase;

        normal.inputs[0].output_index = 0;

        if (is_coinbase_transaction(normal)) {
            std::cerr
                << "[FAIL] Normal input mistaken for coinbase\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Normal input distinguished from coinbase\n";

        Transaction wrong_height =
            make_coinbase_transaction(
                2,
                "CZ1-miner");

        if (validate_coinbase_transaction(
                wrong_height,
                1)) {

            std::cerr
                << "[FAIL] Wrong-height coinbase rejected\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Wrong-height coinbase rejected\n";

        Transaction second =
            make_coinbase_transaction(
                1,
                "CZ1-second-miner");

        std::vector<Transaction> many{
            coinbase,
            second
        };

        if (validate_coinbase_position_and_reward(
                many,
                1)) {

            std::cerr
                << "[FAIL] Multiple coinbases accepted\n";
            return EXIT_FAILURE;
        }

        std::cout
            << "[PASS] Multiple coinbases rejected\n";

        std::cout
            << "ALL COINBASE TESTS PASSED\n";

        return EXIT_SUCCESS;

    } catch (const std::exception& e) {
        std::cerr
            << "[FAIL] "
            << e.what()
            << '\n';

        return EXIT_FAILURE;
    }
}
