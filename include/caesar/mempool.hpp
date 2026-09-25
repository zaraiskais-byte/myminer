#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <caesar/crypto.hpp>
#include <caesar/mempool_validation.hpp>
#include <caesar/transaction.hpp>
#include <caesar/utxo.hpp>
#include "caesar/ownership.hpp"

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
        const UTXOSet& chain_utxos) {

        const Hash256 id = tx.txid();

        if (contains(id)) {
            return {
                MempoolRejectReason::Duplicate
            };
        }

        if (transactions_.size() >= MAX_TRANSACTIONS) {
            return {
                MempoolRejectReason::MempoolFull
            };
        }

        const std::size_t tx_bytes =
            tx.serialize_full_binary().size();

        if (tx_bytes > MAX_TRANSACTION_BYTES) {
            return {
                MempoolRejectReason::TransactionTooLarge
            };
        }

        if (mempool_bytes_ >
            MAX_MEMPOOL_BYTES - tx_bytes) {
            return {
                MempoolRejectReason::MempoolFull
            };
        }

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

        UTXOSet effective_utxos;
        if (!build_effective_utxo(
                chain_utxos,
                effective_utxos)) {
            return {
                MempoolRejectReason::InvalidTransaction
            };
        }

        const auto validation =
            validate_for_mempool(
                tx,
                effective_utxos);

        if (!validation.accepted())
            return validation;

        if (!validate_transaction_witness(
                tx,
                effective_utxos)) {
            return {
                MempoolRejectReason::InvalidTransaction
            };
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

        mempool_bytes_ += tx_bytes;

        return {
            MempoolRejectReason::None
        };
    }

    std::vector<Transaction> ordered_transactions(
        const UTXOSet& chain_utxos) const {

        std::vector<std::pair<Hash256, const Transaction*>>
            pending;

        pending.reserve(transactions_.size());

        for (const auto& entry : transactions_) {
            pending.emplace_back(
                entry.first,
                &entry.second);
        }

        std::sort(
            pending.begin(),
            pending.end(),
            [](const auto& left, const auto& right) {
                return left.first < right.first;
            });

        UTXOSet working = chain_utxos;

        std::vector<Transaction> ordered;
        ordered.reserve(pending.size());

        std::unordered_set<
            Hash256,
            Hash256Hasher> remaining;

        remaining.reserve(pending.size());

        for (const auto& entry : pending)
            remaining.insert(entry.first);

        while (!remaining.empty()) {
            bool progressed = false;

            for (const auto& entry : pending) {
                if (!remaining.contains(entry.first))
                    continue;

                const Transaction& tx =
                    *entry.second;

                bool ready = true;

                for (const auto& input : tx.inputs) {
                    const OutPoint point{
                        input.previous_txid,
                        input.output_index
                    };

                    if (!working.contains(point)) {
                        ready = false;
                        break;
                    }
                }

                if (!ready)
                    continue;

                for (const auto& input : tx.inputs) {
                    const OutPoint point{
                        input.previous_txid,
                        input.output_index
                    };

                    if (!working.spend(point))
                        return {};
                }

                const Hash256 txid = tx.txid();

                for (std::size_t i = 0;
                     i < tx.outputs.size();
                     ++i) {

                    if (i >
                        static_cast<std::size_t>(
                            std::numeric_limits<
                                std::uint32_t>::max())) {
                        return {};
                    }

                    if (!working.add(
                            OutPoint{
                                txid,
                                static_cast<std::uint32_t>(i)
                            },
                            tx.outputs[i])) {
                        return {};
                    }
                }

                ordered.push_back(tx);
                remaining.erase(entry.first);
                progressed = true;
            }

            if (!progressed)
                return {};
        }

        return ordered;
    }

    bool remove(
        const Hash256& txid) {

        if (!transactions_.contains(txid))
            return false;

        std::unordered_set<
            Hash256,
            Hash256Hasher> removal;

        removal.insert(txid);

        bool changed = true;

        while (changed) {
            changed = false;

            for (const auto& entry : transactions_) {
                if (removal.contains(entry.first))
                    continue;

                for (const auto& input :
                     entry.second.inputs) {

                    if (removal.contains(
                            input.previous_txid)) {

                        removal.insert(entry.first);
                        changed = true;
                        break;
                    }
                }
            }
        }

        for (const auto& id : removal) {
            const auto it =
                transactions_.find(id);

            if (it == transactions_.end())
                continue;

            for (const auto& input :
                 it->second.inputs) {

                reserved_inputs_.erase(
                    OutPoint{
                        input.previous_txid,
                        input.output_index
                    });
            }

            mempool_bytes_ -=
                it->second.serialize_full_binary().size();

            transactions_.erase(it);
        }

        return true;
    }

    void clear() {
        transactions_.clear();
        reserved_inputs_.clear();
        mempool_bytes_ = 0;
    }

    std::size_t bytes() const noexcept {
        return mempool_bytes_;
    }

private:
    static constexpr std::size_t MAX_TRANSACTIONS = 5000;
    static constexpr std::size_t MAX_TRANSACTION_BYTES = 1000000;
    static constexpr std::size_t MAX_MEMPOOL_BYTES =
        16 * 1024 * 1024;

    bool build_effective_utxo(
        const UTXOSet& chain_utxos,
        UTXOSet& result) const {

        result = chain_utxos;

        const auto ordered =
            ordered_transactions(chain_utxos);

        if (ordered.size() != transactions_.size())
            return false;

        for (const auto& tx : ordered) {
            for (const auto& input : tx.inputs) {
                if (!result.spend(
                        OutPoint{
                            input.previous_txid,
                            input.output_index
                        })) {
                    return false;
                }
            }

            const Hash256 txid = tx.txid();

            for (std::size_t i = 0;
                 i < tx.outputs.size();
                 ++i) {

                if (i >
                    static_cast<std::size_t>(
                        std::numeric_limits<
                            std::uint32_t>::max())) {
                    return false;
                }

                if (!result.add(
                        OutPoint{
                            txid,
                            static_cast<std::uint32_t>(i)
                        },
                        tx.outputs[i])) {
                    return false;
                }
            }
        }

        return true;
    }

    std::unordered_map<
        Hash256,
        Transaction,
        Hash256Hasher> transactions_;

    std::unordered_set<
        OutPoint,
        OutPointHasher> reserved_inputs_;

    std::size_t mempool_bytes_ = 0;
};

} // namespace caesar
