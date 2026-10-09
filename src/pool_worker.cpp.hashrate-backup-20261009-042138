// Caesar CZR pool worker client
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <caesar/consensus.hpp>
#include <caesar/block.hpp>

namespace {

struct HttpResult { int status = 0; std::string body; bool ok = false; };

bool parse_host_port(const std::string& url, std::string& host, std::uint16_t& port) {
    std::string s = url;
    if (s.rfind("http://", 0) == 0) s = s.substr(7);
    auto colon = s.find(':');
    auto slash = s.find('/');
    if (colon == std::string::npos) { host = s.substr(0, slash); port = 80; }
    else {
        host = s.substr(0, colon);
        std::string p = (slash == std::string::npos) ? s.substr(colon + 1)
                        : s.substr(colon + 1, slash - colon - 1);
        port = static_cast<std::uint16_t>(std::stoi(p));
    }
    return !host.empty() && port != 0;
}

HttpResult http_request(const std::string& host, std::uint16_t port,
                        const std::string& method, const std::string& path,
                        const std::string& body) {
    HttpResult r;
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return r;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        if (host == "localhost") addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        else { ::close(fd); return r; }
    }
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr) < 0) {
        ::close(fd); return r;
    }
    std::ostringstream req;
    req << method << " " << path << " HTTP/1.1\r\nHost: " << host << ":" << port << "\r\n";
    if (method == "POST") {
        req << "Content-Type: application/json\r\nContent-Length: " << body.size() << "\r\n";
    }
    req << "Connection: close\r\n\r\n";
    if (method == "POST") req << body;
    std::string s = req.str();
    ::send(fd, s.data(), s.size(), 0);
    std::string resp;
    char buf[4096];
    ssize_t n;
    while ((n = ::recv(fd, buf, sizeof buf, 0)) > 0) resp.append(buf, n);
    ::close(fd);
    auto hdr_end = resp.find("\r\n\r\n");
    if (hdr_end == std::string::npos) return r;
    auto sp1 = resp.find(' ');
    if (sp1 != std::string::npos) r.status = std::stoi(resp.substr(sp1 + 1, 3));
    r.body = resp.substr(hdr_end + 4);
    r.ok = (r.status == 200);
    return r;
}

std::string json_str(const std::string& b, const std::string& k) {
    auto p = b.find("\"" + k + "\"");
    if (p == std::string::npos) return "";
    p = b.find(':', p);
    if (p == std::string::npos) return "";
    auto s = b.find('"', p);
    if (s == std::string::npos) return "";
    auto e = b.find('"', s + 1);
    if (e == std::string::npos) return "";
    return b.substr(s + 1, e - s - 1);
}

std::uint64_t json_u64(const std::string& b, const std::string& k) {
    auto p = b.find("\"" + k + "\"");
    if (p == std::string::npos) return 0;
    p = b.find(':', p);
    if (p == std::string::npos) return 0;
    auto s = b.find_first_of("0123456789", p);
    if (s == std::string::npos) return 0;
    auto e = b.find_first_not_of("0123456789", s);
    if (e == std::string::npos) e = b.size();
    try { return std::stoull(b.substr(s, e - s)); } catch (...) { return 0; }
}

std::vector<std::uint8_t> hex_to_bytes(const std::string& h) {
    std::vector<std::uint8_t> out;
    out.reserve(h.size() / 2);
    auto val = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (std::size_t i = 0; i + 1 < h.size(); i += 2) {
        int hi = val(h[i]);
        int lo = val(h[i + 1]);
        if (hi < 0 || lo < 0) break;
        out.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
    }
    return out;
}

} // namespace

int main(int argc, char** argv) {
    std::cout.setf(std::ios::unitbuf);
    std::string pool_url, address, name = "worker";
    int threads = 1;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if      (a == "--pool"    && i + 1 < argc) pool_url = argv[++i];
        else if (a == "--address" && i + 1 < argc) address  = argv[++i];
        else if (a == "--name"    && i + 1 < argc) name     = argv[++i];
        else if (a == "--threads" && i + 1 < argc) threads  = std::stoi(argv[++i]);
    }
    if (pool_url.empty() || address.empty()) {
        std::cerr << "usage: caesar_worker --pool http://host:port --address CZ1... [--threads N]\n";
        return 1;
    }
    std::string host;
    std::uint16_t port = 0;
    if (!parse_host_port(pool_url, host, port)) {
        std::cerr << "[worker] invalid pool url\n"; return 1;
    }
    std::cout << "[worker] pool = " << host << ":" << port << "\n";
    std::cout << "[worker] address = " << address << "\n";
    std::cout << "[worker] threads = " << threads << "\n";
    {
        std::string body = "{\"address\":\"" + address + "\",\"worker\":\"" + name + "\"}";
        HttpResult r = http_request(host, port, "POST", "/api/pool/register", body);
        if (!r.ok) {
            std::cerr << "[worker] register failed: " << r.status << " " << r.body << "\n";
            return 1;
        }
        std::cout << "[worker] registered\n";
    }
    std::atomic<bool> stop{false};
    std::atomic<std::uint64_t> accepted{0};
    std::atomic<std::uint64_t> submitted{0};
    auto loop = [&](int tid) {
        while (!stop.load()) {
            HttpResult jr = http_request(host, port, "GET",
                                         "/api/pool/job?address=" + address, "");
            if (!jr.ok) { std::this_thread::sleep_for(std::chrono::seconds(2)); continue; }
            std::uint64_t job_id = json_u64(jr.body, "job_id");
            std::uint32_t share_diff = static_cast<std::uint32_t>(
                json_u64(jr.body, "share_difficulty"));
            std::string header_hex = json_str(jr.body, "header_hex");
            if (job_id == 0 || header_hex.empty()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }
            auto header_bytes = hex_to_bytes(header_hex);
            std::uint64_t nonce = static_cast<std::uint64_t>(tid) * 1000003ULL;
            std::uint64_t tested = 0;
            while (!stop.load() && tested < 50000000ULL) {
                caesar::Hash256 h = caesar::calculate_pow_hash(header_bytes, nonce);
                if (caesar::pow_meets_difficulty(h, share_diff)) {
                    std::ostringstream sb;
                    sb << "{\"address\":\"" << address << "\""
                       << ",\"job_id\":" << job_id
                       << ",\"nonce\":" << nonce << "}";
                    HttpResult sr = http_request(host, port, "POST",
                                                 "/api/pool/submit", sb.str());
                    submitted.fetch_add(1);
                    if (sr.ok && sr.body.find("\"credited\":true") != std::string::npos) {
                        accepted.fetch_add(1);
                    }
                    std::cout << "[worker:" << tid << "] nonce=" << nonce
                              << " acc=" << accepted.load()
                              << " total=" << submitted.load() << std::endl;
                    if (sr.body.find("\"stale\":true") != std::string::npos) {
                        std::cout << "[worker:" << tid
                                  << "] stale job detected, requesting new job"
                                  << std::endl;
                        break;
                    }
                }
                ++nonce; ++tested;
            }
        }
    };
    std::vector<std::thread> ts;
    for (int i = 0; i < threads; ++i) ts.emplace_back(loop, i);
    for (auto& t : ts) t.join();
    std::cout << "[worker] done. accepted=" << accepted.load() << "\n";
    return 0;
}
