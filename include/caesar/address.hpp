#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <caesar/crypto.hpp>

namespace caesar {

inline std::string address_checksum(
    const std::string& payload) {

    const Hash256 first = sha256(payload);
    const Hash256 second =
        sha256(hash_to_hex(first));

    return hash_to_hex(second).substr(0, 8);
}

inline std::string address_from_public_key(
    const std::string& public_key) {

    const Hash256 hash = sha256(public_key);

    const std::string payload =
        "CZ1" + hash_to_hex(hash);

    return payload + address_checksum(payload);
}

inline bool is_valid_address(
    const std::string& address) {

    if (address.size() != 75)
        return false;

    if (address.rfind("CZ1", 0) != 0)
        return false;

    for (std::size_t i = 3; i < address.size(); ++i) {
        const char c = address[i];

        const bool hex =
            (c >= '0' && c <= '9') ||
            (c >= 'a' && c <= 'f');

        if (!hex)
            return false;
    }

    const std::string payload =
        address.substr(0, 67);

    const std::string checksum =
        address.substr(67, 8);

    return checksum == address_checksum(payload);
}

} // namespace caesar
