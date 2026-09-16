#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <caesar/crypto.hpp>
#include <caesar/mempool_validation.hpp>
#include <caesar/transaction.hpp>
#include <caesar/utxo.hpp>

namespace caesar {

class Mempool {
private:
    struct Hash256Hasher {
        std::size_t operator()(
            const Hash256& hash) const noexcept {

            std::size_t h = 0;

            for (std::uint8_t byte : hash) {
                h ^= static_cast<std::size_t>(byte)
                     + 0x9e3779b9u
                     + (h << 6)
                     + (h >> 2);
            }

            return h;
        }
    };

public:
    Mempool() = default;

    bool contains(const Hash256& txid) const {
        return transactions_.find(txid) !=
               transactions_.end();
    }

    std::size_t size() const {
        return transactions_.size();
    }

    bool input_reserved(
        const OutPoint& point) const {

        return reserved_inputs_.find(point) !=
               reserved_inputs_.end();
    }

    const Transaction* get(
        const Hash256& txid) const {

        const auto it =
            transactions_.find(txid);

        if (it == transactions_.end())
            return nullptr;

        return &it->second;
    }

    const std::unordered_map<
        Hash256,
        Transaction,
        Hash256Hasher>& transactions() const {

        return transactions_;
    }

    MempoolValidationResult accept(
        const Transaction& tx,
        const UTXOSet& utxos) {

        const Hash256 id = tx.txid();

        if (contains(id)) {
            return {
                MempoolRejectReason::InvalidTransaction
            };
        }

        const auto validation =
            validate_for_mempool(tx, utxos);

        if (!validation.accepted())
            return validation;

        for (const auto& input : tx.inputs) {

            const OutPoint point{
                input.previous_txid,
                input.output_index
            };

            if (input_reserved(point)) {
                return {
                    MempoolRejectReason::DoubleSpend
                };
            }
        }

        const auto [it, inserted] =
            transactions_.emplace(id, tx);

        if (!inserted) {
            return {
                MempoolRejectReason::InvalidTransaction
            };
        }

        for (const auto& input : tx.inputs) {

            const OutPoint point{
                input.previous_txid,
                input.output_index
            };

            reserved_inputs_.insert(point);
        }

        return {
            MempoolRejectReason::None
        };
    }

    bool remove(
        const Hash256& txid) {

        const auto it =
            transactions_.find(txid);

        if (it == transactions_.end())
            return false;

        for (const auto& input :
             it->second.inputs) {

            const OutPoint point{
                input.previous_txid,
                input.output_index
            };

            reserved_inputs_.erase(point);
        }

        transactions_.erase(it);

        return true;
    }

    void clear() {
        transactions_.clear();
        reserved_inputs_.clear();
    }

private:
    std::unordered_map<
        Hash256,
        Transaction,
        Hash256Hasher> transactions_;

    std::unordered_set<
        OutPoint,
        OutPointHasher> reserved_inputs_;
};

} // namespace caesar
