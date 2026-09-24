#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <unordered_set>

#include <caesar/transaction.hpp>
#include <caesar/utxo.hpp>

namespace caesar {

enum class MempoolRejectReason {
    None,
    InvalidTransaction,
    MissingInput,
    DuplicateInput,
    DoubleSpend,
    OutputOverflow,
    InsufficientInputValue,
    TransactionTooLarge,
    MempoolFull
};

struct MempoolValidationResult {
    MempoolRejectReason reason{
        MempoolRejectReason::None
    };

    bool accepted() const {
        return reason == MempoolRejectReason::None;
    }
};

inline MempoolValidationResult validate_for_mempool(
    const Transaction& tx,
    const UTXOSet& utxos) {

    if (!tx.validate()) {
        return {
            MempoolRejectReason::InvalidTransaction
        };
    }

    std::unordered_set<
        OutPoint,
        OutPointHasher> seen_inputs;

    std::uint64_t input_sum = 0;

    for (const auto& input : tx.inputs) {

        const OutPoint point{
            input.previous_txid,
            input.output_index
        };

        // Reject the same input twice inside one transaction.
        if (!seen_inputs.insert(point).second) {
            return {
                MempoolRejectReason::DuplicateInput
            };
        }

        // Every input must reference an existing unspent output.
        if (!utxos.contains(point)) {
            return {
                MempoolRejectReason::MissingInput
            };
        }

        const TransactionOutput* previous =
            utxos.get(point);

        if (!previous) {
            return {
                MempoolRejectReason::MissingInput
            };
        }

        if (previous->amount >
            std::numeric_limits<std::uint64_t>::max()
            - input_sum) {

            return {
                MempoolRejectReason::OutputOverflow
            };
        }

        input_sum += previous->amount;
    }

    std::uint64_t output_sum = 0;

    for (const auto& output : tx.outputs) {

        if (output.amount == 0 ||
            output.recipient.empty()) {

            return {
                MempoolRejectReason::InvalidTransaction
            };
        }

        if (output.amount >
            std::numeric_limits<std::uint64_t>::max()
            - output_sum) {

            return {
                MempoolRejectReason::OutputOverflow
            };
        }

        output_sum += output.amount;
    }

    if (input_sum < output_sum) {
        return {
            MempoolRejectReason::InsufficientInputValue
        };
    }

    return {
        MempoolRejectReason::None
    };
}

inline const char* mempool_reject_reason_string(
    MempoolRejectReason reason) {

    switch (reason) {
        case MempoolRejectReason::None:
            return "none";

        case MempoolRejectReason::InvalidTransaction:
            return "invalid transaction";

        case MempoolRejectReason::MissingInput:
            return "missing input";

        case MempoolRejectReason::DuplicateInput:
            return "duplicate input";

        case MempoolRejectReason::DoubleSpend:
            return "double spend";

        case MempoolRejectReason::OutputOverflow:
            return "value overflow";

        case MempoolRejectReason::InsufficientInputValue:
            return "insufficient input value";

        case MempoolRejectReason::TransactionTooLarge:
            return "transaction too large";

        case MempoolRejectReason::MempoolFull:
            return "mempool full";
    }

    return "unknown";
}

} // namespace caesar
