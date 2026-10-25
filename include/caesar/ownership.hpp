// Copyright (c) 2026 zaraiskais-byte. All rights reserved.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <caesar/address.hpp>
#include <caesar/key_encoding.hpp>
#include <caesar/signature.hpp>
#include <caesar/transaction.hpp>
#include <caesar/transaction_signature.hpp>
#include <caesar/utxo.hpp>

namespace caesar {

inline std::string public_key_to_address(const std::string& public_key) {
    return address_from_public_key(public_key);
}

inline bool verify_utxo_ownership(const TransactionOutput& previous_output,
                                  const std::string& public_key) {
    if (previous_output.recipient.empty() || public_key.empty()) {
        return false;
    }

    return previous_output.recipient == public_key_to_address(public_key);
}

inline bool validate_signed_transaction_ownership(
    const Transaction& tx, const UTXOSet& utxos, const std::vector<EVP_PKEY*>& public_keys,
    const std::vector<std::string>& public_key_data,
    const std::vector<std::vector<unsigned char>>& signatures);

inline bool validate_transaction_witness(const Transaction& tx, const UTXOSet& utxos) {
    if (!tx.validate())
        return false;

    if (tx.witness.inputs.size() != tx.inputs.size())
        return false;

    std::vector<EVP_PKEY*> public_keys;
    std::vector<std::string> public_key_data;
    std::vector<std::vector<unsigned char>> signatures;

    public_keys.reserve(tx.inputs.size());
    public_key_data.reserve(tx.inputs.size());
    signatures.reserve(tx.inputs.size());

    for (const auto& witness : tx.witness.inputs) {
        if (witness.public_key.empty() || witness.signature.empty())
            goto cleanup;

        // public_key_from_hex() reports failures by throwing,
        // not by returning nullptr. Catch here so that malformed
        // witness data (invalid hex, wrong length) is treated as
        // a validation failure rather than an exception escaping
        // the no-throw contract of validate_transaction_witness().
        // The goto cleanup ensures any EVP_PKEY already collected
        // in public_keys is released before returning false.
        EVP_PKEY* key = nullptr;
        try {
            key = public_key_from_hex(witness.public_key);
        } catch (const std::exception&) {
            goto cleanup;
        }

        public_keys.push_back(key);
        public_key_data.push_back(witness.public_key);
        signatures.push_back(witness.signature);
    }

    {
        const bool valid = validate_signed_transaction_ownership(tx, utxos, public_keys,
                                                                 public_key_data, signatures);

        for (EVP_PKEY* key : public_keys)
            EVP_PKEY_free(key);

        return valid;
    }

cleanup:
    for (EVP_PKEY* key : public_keys)
        EVP_PKEY_free(key);

    return false;
}

inline bool validate_signed_transaction_ownership(
    const Transaction& tx, const UTXOSet& utxos, const std::vector<EVP_PKEY*>& public_keys,
    const std::vector<std::string>& public_key_data,
    const std::vector<std::vector<unsigned char>>& signatures) {
    if (!tx.validate())
        return false;

    if (public_keys.size() != tx.inputs.size())
        return false;

    if (public_key_data.size() != tx.inputs.size())
        return false;

    if (signatures.size() != tx.inputs.size())
        return false;

    if (!validate_transaction_against_utxo(tx, utxos))
        return false;

    for (std::size_t i = 0; i < tx.inputs.size(); ++i) {
        const OutPoint point{tx.inputs[i].previous_txid, tx.inputs[i].output_index};

        const TransactionOutput* previous = utxos.get(point);

        if (!previous)
            return false;

        if (!verify_utxo_ownership(*previous, public_key_data[i])) {
            return false;
        }

        if (!verify_transaction_input_signature(tx, i, public_keys[i], signatures[i])) {
            return false;
        }
    }

    return true;
}

} // namespace caesar
