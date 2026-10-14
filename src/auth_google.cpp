#include <httplib.h>
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstdio>
#include <random>
#include <algorithm>

static std::string url_encode(const std::string &value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;
    for (unsigned char c : value) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else {
            escaped << '%' << std::setw(2) << int(c);
        }
    }
    return escaped.str();
}

static std::string base64url_encode(const std::string &in) {
    static const char lookup[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string out;
    int val = 0, valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(lookup[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(lookup[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

static std::string sha256_raw(const std::string &data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, data.c_str(), data.size());
    SHA256_Final(hash, &ctx);
    return std::string((char*)hash, SHA256_DIGEST_LENGTH);
}

static std::string random_string(size_t len) {
    static const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(chars) - 2);
    std::string s;
    for (size_t i = 0; i < len; ++i) s.push_back(chars[dis(gen)]);
    return s;
}

static std::string derive_wallet(const std::string &email, const std::string &extra = "") {
    std::string seed = email + extra;
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, seed.c_str(), seed.size());
    SHA256_Final(hash, &ctx);
    std::ostringstream oss;
    oss << "CZ1";
    for (int i = 0; i < 20; ++i) oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    return oss.str();
}

static std::string shell_escape(const std::string &s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    out += "'";
    return out;
}

static std::string https_post(const std::string &url, const std::string &body) {
    std::string cmd = "curl -s -X POST " + shell_escape(url) +
                      " -H 'Content-Type: application/x-www-form-urlencoded'"
                      " --data " + shell_escape(body) + " 2>/dev/null";
    FILE *fp = popen(cmd.c_str(), "r");
    if (!fp) return "";
    std::string out;
    char buf[4096];
    while (fgets(buf, sizeof(buf), fp)) out += buf;
    pclose(fp);
    return out;
}

static std::string base64_decode_std(const std::string &in) {
    static const std::string b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T[(unsigned char)b64[i]] = i;
    std::string out;
    int val = 0, valb = -8;
    for (unsigned char c : in) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

void register_google_auth_routes(httplib::Server &svr,
                                  const std::string &client_id,
                                  const std::string &client_secret,
                                  const std::string &redirect_uri) {
    svr.Get("/auth/google", [client_id, redirect_uri](const httplib::Request&, httplib::Response &res) {
        if (client_id.empty()) {
            res.set_content("GOOGLE_CLIENT_ID not set", "text/plain");
            return;
        }
        std::string verifier = random_string(64);
        std::string challenge = base64url_encode(sha256_raw(verifier));
        std::string url = "https://accounts.google.com/o/oauth2/v2/auth?"
            "response_type=code&"
            "client_id=" + url_encode(client_id) + "&"
            "redirect_uri=" + url_encode(redirect_uri) + "&"
            "scope=" + url_encode("openid email profile") + "&"
            "code_challenge=" + challenge + "&"
            "code_challenge_method=S256&"
            "access_type=offline&"
            "prompt=consent";
        res.set_redirect(url);
    });

    svr.Get("/auth/google/callback", [client_id, client_secret, redirect_uri](const httplib::Request &req, httplib::Response &res) {
        auto code = req.get_param_value("code");
        if (code.empty()) {
            res.status = 400;
            res.set_content("Missing authorization code", "text/plain");
            return;
        }
        std::string body = "code=" + code +
            "&client_id=" + client_id +
            "&client_secret=" + client_secret +
            "&redirect_uri=" + redirect_uri +
            "&grant_type=authorization_code";
        std::string resp = https_post("https://oauth2.googleapis.com/token", body);
        if (resp.empty()) {
            res.status = 500;
            res.set_content("Token exchange failed (empty response)", "text/plain");
            return;
        }
        std::string email;
        auto pos = resp.find("\"id_token\":\"");
        if (pos != std::string::npos) {
            pos += 12;
            auto end = resp.find("\"", pos);
            std::string id_token = resp.substr(pos, end - pos);
            auto dot1 = id_token.find('.');
            auto dot2 = id_token.find('.', dot1 + 1);
            if (dot1 != std::string::npos && dot2 != std::string::npos) {
                std::string payload = id_token.substr(dot1 + 1, dot2 - dot1 - 1);
                std::string padded = payload;
                while (padded.size() % 4) padded += '=';
                std::replace(padded.begin(), padded.end(), '-', '+');
                std::replace(padded.begin(), padded.end(), '_', '/');
                std::string decoded = base64_decode_std(padded);
                auto epos = decoded.find("\"email\":\"");
                if (epos != std::string::npos) {
                    epos += 9;
                    auto eend = decoded.find("\"", epos);
                    email = decoded.substr(epos, eend - epos);
                }
            }
        }
        if (email.empty()) {
            res.status = 500;
            res.set_content("Could not extract email from id_token", "text/plain");
            return;
        }
        std::string wallet1 = derive_wallet(email);
        std::string wallet2 = derive_wallet(email, "secondary");
        std::string html = "<html><body style='font-family:sans-serif;background:#0a0e17;color:#e0e0e0;padding:2em'>"
            "<h1>Caesar CZR Wallet</h1>"
            "<p>Signed in as: <b>" + email + "</b></p>"
            "<h2>Wallet 1</h2><code>" + wallet1 + "</code>"
            "<h2>Wallet 2</h2><code>" + wallet2 + "</code>"
            "<p><a href='/'>Go to Dashboard</a></p>"
            "</body></html>";
        res.set_content(html, "text/html");
    });
}
