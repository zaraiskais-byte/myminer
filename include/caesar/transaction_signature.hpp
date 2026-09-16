#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <caesar/crypto.hpp>
#include <caesar/signature.hpp>
#include <caesar/transaction.hpp>
#include <caesar/utxo.hpp>

namespace caesar {

inline std::string transaction_signing_message(
    const Transaction& tx,
    std::size_t input_index) {

    if (input_index >= tx.inputs.size())
        throw std::out_of_range("input index out of range");

    BinaryWriter writer;

    // Domain separator: prevents this signature payload
    // from being confused with another Caesar object.
    writer.write_string("CAESAR_TX_INPUT_SIGNATURE_V1");

    // Sign the canonical binary transaction.
    writer.write_bytes(tx.serialize_binary());

    // Bind the signature to the exact input being authorized.
    if (input_index >
        static_cast<std::size_t>(UINT32_MAX)) {

        throw std::out_of_range(
            "input index exceeds protocol limit");
    }

    writer.write_u32(
        static_cast<std::uint32_t>(input_index));

    return bytes_to_binary_string(writer.data());
}

inline std::vector<unsigned char> sign_transaction_input(
    const Transaction& tx,
    std::size_t input_index,
    EVP_PKEY* private_key) {

    return sign_message(
        private_key,
        transaction_signing_message(
            tx,
            input_index));
}

inline bool verify_transaction_input_signature(
    const Transaction& tx,
    std::size_t input_index,
    EVP_PKEY* public_key,
    const std::vector<unsigned char>& signature) {

    if (input_index >= tx.inputs.size())
        return false;

    return verify_signature(
        public_key,
        transaction_signing_message(
            tx,
            input_index),
        signature);
}

inline bool validate_signed_transaction(
    const Transaction& tx,
    const UTXOSet& utxos,
    const std::vector<EVP_PKEY*>& public_keys,
    const std::vector<std::vector<unsigned char>>& signatures) {

    if (!tx.validate())
        return false;

    if (public_keys.size() != tx.inputs.size())
        return false;

    if (signatures.size() != tx.inputs.size())
        return false;

    if (!validate_transaction_against_utxo(
            tx,
            utxos)) {
        return false;
    }

    for (std::size_t i = 0;
         i < tx.inputs.size();
         ++i) {

        if (!verify_transaction_input_signature(
                tx,
                i,
                public_keys[i],
                signatures[i])) {

            return false;
        }
    }

    return true;
}

} // namespace caesar
