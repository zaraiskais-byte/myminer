#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

namespace caesar {

// Generate secure random entropy
inline std::vector<std::uint8_t> secure_random(std::size_t len) {
    std::vector<std::uint8_t> buf(len);
    if (RAND_bytes(buf.data(), static_cast<int>(len)) != 1) {
        throw std::runtime_error("RAND_bytes failed");
    }
    return buf;
}

// Derive PIN hash (PBKDF2-SHA256, 100k iterations)
inline std::string hash_pin(const std::string& pin, const std::vector<std::uint8_t>& salt) {
    std::vector<std::uint8_t> out(32);
    if (PKCS5_PBKDF2_HMAC(pin.data(), static_cast<int>(pin.size()),
                          salt.data(), static_cast<int>(salt.size()),
                          100000, EVP_sha256(), 32, out.data()) != 1) {
        throw std::runtime_error("PBKDF2 failed");
    }
    std::ostringstream hex;
    hex << std::hex;
    for (auto b : out) {
        hex << (b < 16 ? "0" : "") << static_cast<int>(b);
    }
    return hex.str();
}

// Save PIN hash to file
inline void save_pin(const std::filesystem::path& path, const std::string& pin) {
    auto salt = secure_random(16);
    auto hash = hash_pin(pin, salt);

    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(salt.data()), salt.size());
    f.write(hash.data(), hash.size());
}

// Verify PIN
inline bool verify_pin(const std::filesystem::path& path, const std::string& pin) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    std::vector<std::uint8_t> salt(16);
    f.read(reinterpret_cast<char*>(salt.data()), 16);

    std::string stored_hash(64, '\0');
    f.read(&stored_hash[0], 64);

    return hash_pin(pin, salt) == stored_hash;
}

}  // namespace caesar
