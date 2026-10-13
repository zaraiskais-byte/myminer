#pragma once

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>

namespace caesar {

struct SmtpConfig {
    std::string host = "smtp.gmail.com";
    int port = 465;
    std::string username;
    std::string password;
    std::string from_address;
    std::string from_name = "Caesar CZR";
    bool enabled = false;
};

class SmtpSender {
public:
    SmtpSender() = default;

    void load_config(const std::filesystem::path& path) {
        std::lock_guard<std::mutex> lk(mutex_);
        config_path_ = path;
        config_ = SmtpConfig{};
        std::ifstream f(path);
        if (!f) return;
        std::string line;
        while (std::getline(f, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line[0] == '#') continue;
            auto pos = line.find('=');
            if (pos == std::string::npos) continue;
            std::string key = line.substr(0, pos);
            std::string val = line.substr(pos + 1);
            if (key == "host") config_.host = val;
            else if (key == "port") { try { config_.port = std::stoi(val); } catch (...) {} }
            else if (key == "username") config_.username = val;
            else if (key == "password") config_.password = val;
            else if (key == "from_address") config_.from_address = val;
            else if (key == "from_name") config_.from_name = val;
        }
        config_.enabled = !config_.username.empty()
                       && !config_.password.empty()
                       && !config_.from_address.empty();
        if (config_.enabled) {
            std::fprintf(stderr, "[SMTP] config loaded host=%s port=%d user=%s\n",
                config_.host.c_str(), config_.port, config_.username.c_str());
        }
    }

    bool is_enabled() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return config_.enabled;
    }

    bool send(const std::string& to, const std::string& subject, const std::string& body) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!config_.enabled) return false;

        auto quote = [](const std::string& s) -> std::string {
            std::string out = "'";
            for (char c : s) {
                if (c == '\'') out += "'\\''";
                else out += c;
            }
            out += "'";
            return out;
        };

        std::filesystem::path msg_path = config_path_.parent_path() / "outgoing.eml";
        {
            std::ofstream mf(msg_path, std::ios::trunc);
            if (!mf) return false;
            mf << "From: " << config_.from_name << " <" << config_.from_address << ">\r\n";
            mf << "To: <" << to << ">\r\n";
            mf << "Subject: " << subject << "\r\n";
            mf << "MIME-Version: 1.0\r\n";
            mf << "Content-Type: text/plain; charset=UTF-8\r\n";
            mf << "\r\n";
            mf << body << "\r\n";
        }

        std::ostringstream cmd;
        cmd << "curl -s --url 'smtps://" << config_.host << ":" << config_.port << "'";
        cmd << " --mail-from " << quote(config_.from_address);
        cmd << " --mail-rcpt " << quote(to);
        cmd << " --user " << quote(config_.username + ":" + config_.password);
        cmd << " --ssl-reqd";
        cmd << " --upload-file " << quote(msg_path.string());
        cmd << " 2>&1";

        FILE* p = popen(cmd.str().c_str(), "r");
        if (!p) return false;
        char buf[512];
        std::string output;
        while (std::fgets(buf, sizeof(buf), p)) output += buf;
        int rc = pclose(p);
        std::error_code ec;
        std::filesystem::remove(msg_path, ec);

        if (rc != 0) {
            std::fprintf(stderr, "[SMTP] curl failed rc=%d output=%s\n", rc, output.c_str());
            return false;
        }
        std::fprintf(stderr, "[SMTP] sent to %s\n", to.c_str());
        return true;
    }

private:
    SmtpConfig config_;
    std::filesystem::path config_path_;
    mutable std::mutex mutex_;
};

}  // namespace caesar
