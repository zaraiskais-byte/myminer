#include <iostream>

#include <caesar/coinbase.hpp>

using namespace caesar;

int main() {
    const auto a =
        make_coinbase_transaction(
            1,
            "CZ1-miner");

    const auto b =
        make_coinbase_transaction(
            2,
            "CZ1-miner");

    const auto c =
        make_coinbase_transaction(
            1,
            "CZ1-miner",
            1);

    if (a.txid() == b.txid()) {
        std::cerr << "FAIL: height did not change txid\n";
        return 1;
    }

    if (a.txid() == c.txid()) {
        std::cerr << "FAIL: extra nonce did not change txid\n";
        return 1;
    }

    if (!validate_coinbase_transaction(a, 1)) {
        std::cerr << "FAIL: normal coinbase rejected\n";
        return 1;
    }

    if (!validate_coinbase_transaction(b, 2)) {
        std::cerr << "FAIL: height 2 coinbase rejected\n";
        return 1;
    }

    if (!validate_coinbase_transaction(c, 1, 1)) {
        std::cerr << "FAIL: extra nonce coinbase rejected\n";
        return 1;
    }

    if (validate_coinbase_transaction(b, 1)) {
        std::cerr << "FAIL: wrong height accepted\n";
        return 1;
    }

    std::cout << "COINBASE COMMITMENT TEST PASSED\n";
    return 0;
}
