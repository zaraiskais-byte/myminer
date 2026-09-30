#pragma once
#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace caesar {

// Sliding-window rate limiter + brute-force protection
class RateLimiter {
   public:
    struct Config {
        int max_requests_per_minute = 60;
        int max_failed_logins = 5;
        int ban_duration_seconds = 300;
    };

    RateLimiter() : cfg_{} {}
    explicit RateLimiter(Config c) : cfg_(c) {}

    // Returns true if request allowed
    bool allow(const std::string& ip) {
        std::lock_guard<std::mutex> lk(mu_);
        const auto now = std::chrono::steady_clock::now();

        // Check ban
        auto bit = bans_.find(ip);
        if (bit != bans_.end()) {
            if (now < bit->second) return false;
            bans_.erase(bit);
        }

        auto& entries = windows_[ip];
        // Remove entries older than 60s
        const auto cutoff = now - std::chrono::seconds(60);
        entries.erase(
            std::remove_if(entries.begin(), entries.end(),
                [cutoff](const auto& t) { return t < cutoff; }),
            entries.end());

        if (static_cast<int>(entries.size()) >= cfg_.max_requests_per_minute) {
            return false;
        }
        entries.push_back(now);
        return true;
    }

    void record_failure(const std::string& ip) {
        std::lock_guard<std::mutex> lk(mu_);
        int& count = failures_[ip];
        ++count;
        if (count >= cfg_.max_failed_logins) {
            bans_[ip] = std::chrono::steady_clock::now() +
                std::chrono::seconds(cfg_.ban_duration_seconds);
            failures_.erase(ip);
        }
    }

    void record_success(const std::string& ip) {
        std::lock_guard<std::mutex> lk(mu_);
        failures_.erase(ip);
    }

   private:
    Config cfg_;
    std::mutex mu_;
    std::unordered_map<std::string, std::vector<std::chrono::steady_clock::time_point>> windows_;
    std::unordered_map<std::string, int> failures_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> bans_;
};

}  // namespace caesar
