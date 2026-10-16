#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <caesar/crypto.hpp>

namespace caesar {

inline std::string address_checksum(const std::string& payload) {
    const Hash256 first = sha256(payload);
    const Hash256 second = sha256(hash_to_hex(first));

    return hash_to_hex(second).substr(0, 8);
}

/*
 * Canonicalize a hex-encoded public key to lowercase.
 *
 * The address derivation hashes the *text* of the public key
 * with SHA-256. Without canonicalization, the same Ed25519 key
 * written as "ABCDEF..." and "abcdef..." would produce two
 * different addresses, because the underlying ASCII bytes
 * differ. A key that round-trips through a JSON API, a shell
 * variable, or any tool that uppercases hex would silently
 * derive a different address from the wallet's own.
 *
 * This function only lowercases the A-F characters. It does
 * not reject non-hex input; the current contract accepts any
 * string and this fix preserves that.
 */
inline std::string canonicalize_public_key(const std::string& public_key) {
    std::string out;
    out.reserve(public_key.size());
    for (char c : public_key) {
        if (c >= 'A' && c <= 'F')
            out.push_back(static_cast<char>(c - 'A' + 'a'));
        else
            out.push_back(c);
    }
    return out;
}

inline std::string address_from_public_key(const std::string& public_key) {
    const Hash256 hash = sha256(canonicalize_public_key(public_key));

    const std::string payload = "CZ1" + hash_to_hex(hash);

    return payload + address_checksum(payload);
}

inline bool is_valid_address(const std::string& address) {
    if (address.size() != 75)
        return false;

    if (address.rfind("CZ1", 0) != 0)
        return false;

    for (std::size_t i = 3; i < address.size(); ++i) {
        const char c = address[i];

        const bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');

        if (!hex)
            return false;
    }

    const std::string payload = address.substr(0, 67);

    const std::string checksum = address.substr(67, 8);

    return checksum == address_checksum(payload);
}

} // namespace caesar
