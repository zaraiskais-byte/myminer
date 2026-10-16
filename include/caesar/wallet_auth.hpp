#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <openssl/crypto.h>
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

    std::array<std::uint8_t, 16> salt{};
    std::array<char, 64> stored_hash{};

    if (!f.read(reinterpret_cast<char*>(salt.data()),
                 static_cast<std::streamsize>(salt.size())) ||
        !f.read(stored_hash.data(),
                static_cast<std::streamsize>(stored_hash.size()))) {
        return false;
    }

    // The established pin.hash format is exactly 16 salt bytes + 64 hex
    // characters. Reject truncated or unexpectedly extended files.
    if (f.peek() != std::char_traits<char>::eof()) {
        return false;
    }
    if (f.bad()) {
        return false;
    }

    const std::string expected = hash_pin(
        pin, std::vector<std::uint8_t>(salt.begin(), salt.end()));

    return expected.size() == stored_hash.size() &&
           CRYPTO_memcmp(expected.data(), stored_hash.data(),
                         stored_hash.size()) == 0;
}

}  // namespace caesar
