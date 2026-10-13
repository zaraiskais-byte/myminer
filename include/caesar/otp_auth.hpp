#pragma once

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>

#include <openssl/rand.h>

namespace caesar {

struct OtpEntry {
    std::string code;
    std::int64_t created_at_unix = 0;
    std::int64_t expires_at_unix = 0;
    int attempts = 0;
};

class OtpStore {
public:
    using Sender = std::function<void(const std::string&, const std::string&)>;

    explicit OtpStore(std::filesystem::path storage_path)
        : storage_path_(std::move(storage_path)) {
        load();
    }

    void set_sender(Sender s) { sender_ = std::move(s); }

    std::string create(const std::string& identifier, int ttl_seconds = 300) {
        std::lock_guard<std::mutex> lk(mutex_);
        OtpEntry entry;
        entry.code = generate_code();
        entry.created_at_unix = now_unix();
        entry.expires_at_unix = entry.created_at_unix + ttl_seconds;
        entry.attempts = 0;
        entries_[identifier] = entry;
        save();
        send(identifier, entry.code);
        return entry.code;
    }

    bool verify(const std::string& identifier, const std::string& code) {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = entries_.find(identifier);
        if (it == entries_.end()) return false;
        OtpEntry& e = it->second;
        if (now_unix() > e.expires_at_unix) {
            entries_.erase(it);
            save();
            return false;
        }
        e.attempts += 1;
        if (e.attempts > 5) {
            entries_.erase(it);
            save();
            return false;
        }
        if (e.code == code) {
            entries_.erase(it);
            save();
            return true;
        }
        save();
        return false;
    }

    static std::string generate_code() {
        std::uint32_t value = 0;
        if (RAND_bytes(reinterpret_cast<unsigned char*>(&value), sizeof(value)) != 1) {
            throw std::runtime_error("RAND_bytes failed");
        }
        std::uint32_t code = value % 1000000u;
        std::ostringstream oss;
        oss.width(6);
        oss.fill('0');
        oss << code;
        return oss.str();
    }

    virtual void send(const std::string& identifier, const std::string& code) {
        if (sender_) {
            sender_(identifier, code);
        } else {
            std::fprintf(stderr, "[OTP] identifier=%s code=%s\n",
                         identifier.c_str(), code.c_str());
        }
    }

private:
    static std::int64_t now_unix() {
        using namespace std::chrono;
        return duration_cast<seconds>(
            system_clock::now().time_since_epoch()).count();
    }

    void load() {
        std::ifstream f(storage_path_);
        if (!f) return;
        std::string line;
        while (std::getline(f, line)) {
            std::istringstream iss(line);
            std::string id;
            OtpEntry e;
            if (iss >> id >> e.code >> e.created_at_unix
                    >> e.expires_at_unix >> e.attempts) {
                entries_[id] = e;
            }
        }
    }

    void save() const {
        if (storage_path_.has_parent_path()) {
            std::filesystem::create_directories(storage_path_.parent_path());
        }
        std::ofstream f(storage_path_, std::ios::trunc);
        for (const auto& kv : entries_) {
            f << kv.first << ' '
              << kv.second.code << ' '
              << kv.second.created_at_unix << ' '
              << kv.second.expires_at_unix << ' '
              << kv.second.attempts << '\n';
        }
    }

    std::filesystem::path storage_path_;
    std::map<std::string, OtpEntry> entries_;
    std::mutex mutex_;
    Sender sender_;
};

}  // namespace caesar
