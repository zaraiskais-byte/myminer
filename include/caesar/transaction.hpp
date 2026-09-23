#pragma once

#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <caesar/crypto.hpp>
#include <caesar/serialization.hpp>
#include <caesar/witness.hpp>

namespace caesar {

struct TransactionInput {
    Hash256 previous_txid{};
    std::uint32_t output_index{};

    std::string serialize() const {
        std::ostringstream out;
        out << hash_to_hex(previous_txid)
            << ':'
            << output_index;
        return out.str();
    }

    std::vector<std::uint8_t> serialize_binary() const {
        BinaryWriter writer;

        for (const std::uint8_t byte : previous_txid)
            writer.write_u8(byte);

        writer.write_u32(output_index);

        return writer.data();
    }

    static TransactionInput deserialize(
        BinaryReader& reader) {

        TransactionInput input;

        const auto hash =
            reader.read_bytes(32);

        for (std::size_t i = 0; i < 32; ++i)
            input.previous_txid[i] = hash[i];

        input.output_index =
            reader.read_u32();

        return input;
    }
};

struct TransactionOutput {
    std::uint64_t amount{};
    std::string recipient;

    std::string serialize() const {
        std::ostringstream out;
        out << amount
            << ':'
            << recipient.size()
            << ':'
            << recipient;
        return out.str();
    }

    std::vector<std::uint8_t> serialize_binary() const {
        BinaryWriter writer;

        writer.write_u64(amount);
        writer.write_string(recipient);

        return writer.data();
    }

    static TransactionOutput deserialize(
        BinaryReader& reader) {

        TransactionOutput output;

        output.amount =
            reader.read_u64();

        output.recipient =
            reader.read_string();

        return output;
    }
};

struct Transaction {
    std::uint32_t version{1};
    std::vector<std::uint8_t> coinbase_data;
    std::vector<TransactionInput> inputs;
    std::vector<TransactionOutput> outputs;
    TransactionWitnessSet witness;

    std::string serialize() const {
        std::ostringstream out;

        out << "TX"
            << '|'
            << version
            << '|'
            << inputs.size();

        for (const auto& input : inputs)
            out << '|' << input.serialize();

        out << '|'
            << outputs.size();

        for (const auto& output : outputs)
            out << '|' << output.serialize();

        return out.str();
    }

    std::vector<std::uint8_t> serialize_binary() const {
        BinaryWriter writer;

        writer.write_u32(version);

        if (coinbase_data.size() > 128)
            throw std::runtime_error("coinbase data too large");

        writer.write_u32(static_cast<std::uint32_t>(coinbase_data.size()));
        writer.write_bytes(coinbase_data);

        if (inputs.size() >
            static_cast<std::size_t>(UINT32_MAX)) {

            throw std::runtime_error(
                "too many transaction inputs");
        }

        writer.write_u32(
            static_cast<std::uint32_t>(
                inputs.size()));

        for (const auto& input : inputs)
            writer.write_bytes(
                input.serialize_binary());

        if (outputs.size() >
            static_cast<std::size_t>(UINT32_MAX)) {

            throw std::runtime_error(
                "too many transaction outputs");
        }

        writer.write_u32(
            static_cast<std::uint32_t>(
                outputs.size()));

        for (const auto& output : outputs)
            writer.write_bytes(
                output.serialize_binary());

        return writer.data();
    }

    std::vector<std::uint8_t> serialize_full_binary() const {
        BinaryWriter writer;
        const auto base = serialize_binary();
        writer.write_bytes(base);

        if (witness.inputs.size() > static_cast<std::size_t>(UINT32_MAX))
            throw std::runtime_error("too many transaction witnesses");

        writer.write_u32(static_cast<std::uint32_t>(witness.inputs.size()));

        for (const auto& item : witness.inputs) {
            writer.write_string(item.public_key);

            if (item.signature.size() > static_cast<std::size_t>(UINT32_MAX))
                throw std::runtime_error("transaction signature too large");

            writer.write_u32(static_cast<std::uint32_t>(item.signature.size()));
            writer.write_bytes(item.signature);
        }

        return writer.data();
    }

    static Transaction deserialize_full(
        const std::vector<std::uint8_t>& data) {

        BinaryReader reader(data);
        Transaction tx;

        tx.version = reader.read_u32();

        const std::uint32_t coinbase_data_size = reader.read_u32();
        if (coinbase_data_size > 128)
            throw std::runtime_error("coinbase data too large");
        tx.coinbase_data = reader.read_bytes(coinbase_data_size);

        const std::uint32_t input_count = reader.read_u32();
        if (input_count > 1000000)
            throw std::runtime_error("too many transaction inputs");

        tx.inputs.reserve(input_count);
        for (std::uint32_t i = 0; i < input_count; ++i)
            tx.inputs.push_back(TransactionInput::deserialize(reader));

        const std::uint32_t output_count = reader.read_u32();
        if (output_count > 1000000)
            throw std::runtime_error("too many transaction outputs");

        tx.outputs.reserve(output_count);
        for (std::uint32_t i = 0; i < output_count; ++i)
            tx.outputs.push_back(TransactionOutput::deserialize(reader));

        const std::uint32_t witness_count = reader.read_u32();
        if (witness_count > 1000000)
            throw std::runtime_error("too many transaction witnesses");

        tx.witness.inputs.reserve(witness_count);
        for (std::uint32_t i = 0; i < witness_count; ++i) {
            TransactionWitness item;
            item.public_key = reader.read_string();
            const std::uint32_t signature_size = reader.read_u32();
            if (signature_size > 1000000)
                throw std::runtime_error("transaction signature too large");
            item.signature = reader.read_bytes(signature_size);
            tx.witness.inputs.push_back(std::move(item));
        }

        if (!reader.empty())
            throw std::runtime_error("trailing bytes after full transaction");

        return tx;
    }

    static Transaction deserialize(
        const std::vector<std::uint8_t>& data) {

        BinaryReader reader(data);

        Transaction tx;

        tx.version =
            reader.read_u32();

        const std::uint32_t coinbase_data_size =
            reader.read_u32();
        if (coinbase_data_size > 128)
            throw std::runtime_error("coinbase data too large");
        tx.coinbase_data = reader.read_bytes(coinbase_data_size);

        const std::uint32_t input_count =
            reader.read_u32();

        if (input_count > 1000000)
            throw std::runtime_error(
                "too many transaction inputs");

        tx.inputs.reserve(input_count);

        for (std::uint32_t i = 0;
             i < input_count;
             ++i) {

            tx.inputs.push_back(
                TransactionInput::deserialize(reader));
        }

        const std::uint32_t output_count =
            reader.read_u32();

        if (output_count > 1000000)
            throw std::runtime_error(
                "too many transaction outputs");

        tx.outputs.reserve(output_count);

        for (std::uint32_t i = 0;
             i < output_count;
             ++i) {

            tx.outputs.push_back(
                TransactionOutput::deserialize(reader));
        }

        if (!reader.empty())
            throw std::runtime_error(
                "trailing bytes after transaction");

        return tx;
    }

    Hash256 txid() const {
        return sha256(
            bytes_to_binary_string(
                serialize_binary()));
    }

    Hash256 wtxid() const {
        return sha256(
            bytes_to_binary_string(
                serialize_full_binary()));
    }

    bool validate() const {
        if (version == 0)
            return false;

        if (inputs.empty() || outputs.empty())
            return false;

        std::uint64_t output_sum = 0;

        for (const auto& output : outputs) {
            if (output.amount == 0)
                return false;

            if (output.recipient.empty())
                return false;

            if (output.amount >
                std::numeric_limits<std::uint64_t>::max()
                - output_sum) {

                return false;
            }

            output_sum += output.amount;
        }

        return output_sum > 0;
    }
};

} // namespace caesar
