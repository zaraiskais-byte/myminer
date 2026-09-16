#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace caesar {

struct TransactionWitness {
    std::string public_key;
    std::vector<unsigned char> signature;
};

struct TransactionWitnessSet {
    std::vector<TransactionWitness> inputs;

    bool empty() const {
        return inputs.empty();
    }

    std::size_t size() const {
        return inputs.size();
    }
};

} // namespace caesar
