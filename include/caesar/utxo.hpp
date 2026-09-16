#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <caesar/transaction.hpp>

namespace caesar {

struct OutPoint {
    Hash256 txid{};
    std::uint32_t output_index{};

    bool operator==(const OutPoint& other) const {
        return txid == other.txid &&
               output_index == other.output_index;
    }
};

struct OutPointHasher {
    std::size_t operator()(const OutPoint& point) const noexcept {
        std::size_t h = 0;

        for (std::uint8_t byte : point.txid)
            h = (h * 131) ^ byte;

        h ^= static_cast<std::size_t>(point.output_index) +
             0x9e3779b9u +
             (h << 6) +
             (h >> 2);

        return h;
    }
};

class UTXOSet {
public:
    bool add(const OutPoint& outpoint,
             const TransactionOutput& output) {
        return entries_.emplace(outpoint, output).second;
    }

    bool contains(const OutPoint& outpoint) const {
        return entries_.find(outpoint) != entries_.end();
    }

    const TransactionOutput* get(const OutPoint& outpoint) const {
        const auto it = entries_.find(outpoint);

        if (it == entries_.end())
            return nullptr;

        return &it->second;
    }

    bool spend(const OutPoint& outpoint) {
        return entries_.erase(outpoint) == 1;
    }

    std::size_t size() const {
        return entries_.size();
    }

private:
    std::unordered_map<OutPoint, TransactionOutput, OutPointHasher> entries_;
};

inline bool validate_transaction_against_utxo(
    const Transaction& tx,
    const UTXOSet& utxos) {

    if (!tx.validate())
        return false;

    std::uint64_t input_sum = 0;
    std::unordered_set<OutPoint, OutPointHasher> seen_inputs;

    for (const auto& input : tx.inputs) {
        const OutPoint point{
            input.previous_txid,
            input.output_index
        };

        if (!seen_inputs.insert(point).second)
            return false;

        const TransactionOutput* previous = utxos.get(point);

        if (!previous)
            return false;

        if (previous->amount >
            std::numeric_limits<std::uint64_t>::max() - input_sum)
            return false;

        input_sum += previous->amount;
    }

    std::uint64_t output_sum = 0;

    for (const auto& output : tx.outputs) {
        if (output.amount >
            std::numeric_limits<std::uint64_t>::max() - output_sum)
            return false;

        output_sum += output.amount;
    }

    return input_sum >= output_sum;
}

} // namespace caesar
