#pragma once

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <openssl/evp.h>

#include <caesar/address.hpp>
#include <caesar/signature.hpp>

namespace caesar {

inline std::string bytes_to_hex(
    const unsigned char* data,
    std::size_t size) {

    if (!data && size != 0)
        throw std::runtime_error("invalid key data");

    std::ostringstream out;
    out << std::hex << std::setfill('0');

    for (std::size_t i = 0; i < size; ++i)
        out << std::setw(2)
            << static_cast<unsigned int>(data[i]);

    return out.str();
}

class Wallet {
public:
    Wallet()
        : keys_(generate_keypair()) {}

    Wallet(const Wallet&) = delete;
    Wallet& operator=(const Wallet&) = delete;

    Wallet(Wallet&&) noexcept = default;
    Wallet& operator=(Wallet&&) noexcept = default;

    std::string public_key() const {
        if (!keys_.public_key)
            throw std::runtime_error(
                "wallet public key unavailable");

        std::size_t size = 0;

        if (EVP_PKEY_get_raw_public_key(
                keys_.public_key,
                nullptr,
                &size) != 1) {

            throw std::runtime_error(
                "failed to determine public key size");
        }

        std::vector<unsigned char> raw(size);

        if (EVP_PKEY_get_raw_public_key(
                keys_.public_key,
                raw.data(),
                &size) != 1) {

            throw std::runtime_error(
                "failed to extract public key");
        }

        raw.resize(size);

        return bytes_to_hex(raw.data(), raw.size());
    }

    std::string address() const {
        return address_from_public_key(public_key());
    }

    bool valid() const {
        if (!keys_.private_key ||
            !keys_.public_key) {

            return false;
        }

        const std::string key = public_key();

        return key.size() == 64 &&
               is_valid_address(address());
    }

    EVP_PKEY* private_key() const {
        return keys_.private_key;
    }

    EVP_PKEY* public_key_handle() const {
        return keys_.public_key;
    }

private:
    KeyPair keys_;
};

} // namespace caesar
