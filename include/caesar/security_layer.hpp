#pragma once
#include <string>

namespace caesar {

// Security headers for HTTP responses
inline void apply_security_headers(httplib::Response& res) {
    res.set_header("X-Content-Type-Options", "nosniff");
    res.set_header("X-Frame-Options", "DENY");
    res.set_header("X-XSS-Protection", "1; mode=block");
    res.set_header("Referrer-Policy", "no-referrer");
    res.set_header("Permissions-Policy", "geolocation=(), microphone=(), camera=()");
    res.set_header("Content-Security-Policy",
        "default-src 'self'; "
        "script-src 'self' 'unsafe-inline'; "
        "style-src 'self' 'unsafe-inline'; "
        "img-src 'self' data:; "
        "connect-src 'self'");
    res.set_header("Strict-Transport-Security", "max-age=31536000");
}

// Input validation helpers
inline bool is_safe_string(const std::string& s, std::size_t max_len = 256) {
    if (s.empty() || s.size() > max_len) return false;
    for (char c : s) {
        if (c == '\0' || c == '\n' || c == '\r') return false;
        if (static_cast<unsigned char>(c) < 32) return false;
    }
    return true;
}

inline bool is_safe_amount(std::uint64_t amount, std::uint64_t max = 1000000000000ULL) {
    return amount > 0 && amount <= max;
}

}  // namespace caesar
