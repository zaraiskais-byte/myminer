#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

#include <httplib.h>

#include <caesar/node.hpp>
#include <caesar/wallet.hpp>

namespace caesar {

inline const char* WALLET_HTML = R"HTML(<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Caesar CZR Wallet</title>
<style>
body{font-family:monospace;background:#0f1115;color:#e8e8e8;padding:16px;margin:0}
h1{color:#f0c040;font-size:20px;margin:0 0 12px}
h2{color:#f0c040;font-size:16px;margin:0 0 8px}
.card{background:#1a1d24;padding:14px;margin:10px 0;border-radius:8px;border:1px solid #252a33}
.val{color:#5fdc7a;font-weight:bold;word-break:break-all}
button{background:#f0c040;color:#000;border:none;padding:10px 16px;font-size:14px;cursor:pointer;margin:4px 0;border-radius:6px;font-weight:bold}
button:active{background:#d0a020}
pre{background:#000;padding:10px;border-radius:4px;font-size:11px;max-height:200px;overflow-y:auto;white-space:pre-wrap;word-break:break-all}
.row{display:flex;justify-content:space-between;padding:4px 0;font-size:14px}
</style>
</head>
<body>
<h1>⚡ Caesar CZR Wallet</h1>

<div class="card">
<h2>Node Status</h2>
<div class="row"><span>Height</span><span class="val" id="height">-</span></div>
<div class="row"><span>Peers</span><span class="val" id="peers">-</span></div>
<div class="row"><span>Mempool</span><span class="val" id="mempool">-</span></div>
<div class="row"><span>Uptime</span><span class="val" id="uptime">-</span></div>
</div>

<div class="card">
<h2>Wallet</h2>
<div class="row"><span>Public Key</span></div>
<div style="padding:4px 0"><span class="val" id="pubkey" style="font-size:11px">-</span></div>
<button onclick="mineToWallet()">⛏️ Mine Block</button>
<button onclick="refreshWallet()">🔄 Refresh</button>
</div>

<div class="card">
<h2>Log</h2>
<pre id="log">Ready.</pre>
</div>

<script>
function log(m){
  var t = new Date().toLocaleTimeString();
  document.getElementById('log').textContent = t+' | '+m+'\n'+document.getElementById('log').textContent;
}
async function refreshStatus(){
  try{
    var r = await fetch('/api/status');
    var d = await r.json();
    document.getElementById('height').textContent = d.height;
    document.getElementById('peers').textContent = d.peers;
    document.getElementById('mempool').textContent = d.mempool;
    document.getElementById('uptime').textContent = d.uptime + 's';
  }catch(e){}
}
async function refreshWallet(){
  try{
    var r = await fetch('/api/wallet');
    var d = await r.json();
    document.getElementById('pubkey').textContent = d.public_key || '-';
    log('Wallet loaded');
  }catch(e){ log('Wallet error: '+e.message); }
}
async function mineToWallet(){
  log('Mining...');
  try{
    var r = await fetch('/api/mine_default', {method:'POST'});
    var d = await r.json();
    if(d.error){ log('Error: '+d.error); return; }
    log('Mined! New height: '+d.height);
    refreshStatus();
  }catch(e){ log('Mine error: '+e.message); }
}
refreshStatus();
refreshWallet();
setInterval(refreshStatus, 3000);
</script>
</body>
</html>)HTML";

class HttpRpcServer {
   public:
    HttpRpcServer(CaesarNode& node, std::uint16_t port)
        : node_(node),
          port_(port),
          started_at_(std::chrono::steady_clock::now()) {
        try {
            wallet_ = std::make_unique<Wallet>();
        } catch (...) {
            wallet_ = nullptr;
        }
        setup_routes();
    }

    HttpRpcServer(const HttpRpcServer&) = delete;
    HttpRpcServer& operator=(const HttpRpcServer&) = delete;

    void start() {
        thread_ = std::thread([this]() { server_.listen("0.0.0.0", port_); });

        for (int i = 0; i < 100; ++i) {
            if (server_.is_running()) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    void stop() {
        server_.stop();
        if (thread_.joinable()) thread_.join();
    }

    bool running() const {
        return server_.is_running();
    }

   private:
    CaesarNode& node_;
    std::uint16_t port_;
    httplib::Server server_;
    std::thread thread_;
    std::chrono::steady_clock::time_point started_at_;
    std::unique_ptr<Wallet> wallet_;

    void setup_routes() {
        server_.Get("/", [](const httplib::Request&, httplib::Response& res) {
            res.set_content(WALLET_HTML, "text/html; charset=utf-8");
        });

        server_.Get("/api/status", [this](const httplib::Request&, httplib::Response& res) {
            auto now = std::chrono::steady_clock::now();
            auto uptime =
                std::chrono::duration_cast<std::chrono::seconds>(now - started_at_).count();
            std::ostringstream out;
            out << "{"
                << "\"height\":" << node_.height() << ","
                << "\"peers\":" << node_.peer_count() << ","
                << "\"mempool\":" << node_.mempool_size() << ","
                << "\"uptime\":" << uptime << "}";
            res.set_content(out.str(), "application/json");
        });

        server_.Get("/api/wallet", [this](const httplib::Request&, httplib::Response& res) {
            std::string pubkey;
            try {
                if (wallet_) pubkey = wallet_->public_key();
            } catch (...) {
                pubkey = "";
            }
            std::string json = "{\"public_key\":\"" + pubkey + "\"}";
            res.set_content(json, "application/json");
        });

        server_.Post("/api/mine_default", [this](const httplib::Request&,
                                                  httplib::Response& res) {
            try {
                std::string recipient = "CAESAR_MINER_CZR1";
                if (wallet_) {
                    try {
                        recipient = wallet_->public_key();
                    } catch (...) {}
                }
                node_.mine_one_block(recipient, 1000000);
                std::ostringstream out;
                out << "{\"status\":\"ok\",\"height\":" << node_.height() << "}";
                res.set_content(out.str(), "application/json");
            } catch (const std::exception& e) {
                res.status = 500;
                std::string msg = "{\"error\":\"" + std::string(e.what()) + "\"}";
                res.set_content(msg, "application/json");
            }
        });

        server_.Post("/api/mine", [this](const httplib::Request& req, httplib::Response& res) {
            try {
                std::string recipient = "CAESAR_MINER_CZR1";
                auto pos = req.body.find("\"recipient\"");
                if (pos != std::string::npos) {
                    auto start = req.body.find("\"", pos + 11);
                    if (start != std::string::npos) {
                        auto end = req.body.find("\"", start + 1);
                        if (end != std::string::npos) {
                            recipient = req.body.substr(start + 1, end - start - 1);
                        }
                    }
                }
                node_.mine_one_block(recipient, 1000000);
                std::ostringstream out;
                out << "{\"status\":\"ok\",\"height\":" << node_.height() << "}";
                res.set_content(out.str(), "application/json");
            } catch (const std::exception& e) {
                res.status = 500;
                std::string msg = "{\"error\":\"" + std::string(e.what()) + "\"}";
                res.set_content(msg, "application/json");
            }
        });

        server_.Get("/api/chain", [this](const httplib::Request&, httplib::Response& res) {
            auto chain = node_.chain();
            std::ostringstream out;
            out << "{\"count\":" << chain.size() << ",\"blocks\":[";
            std::size_t max_show = 10;
            std::size_t start = chain.size() > max_show ? chain.size() - max_show : 0;
            bool first = true;
            for (std::size_t i = start; i < chain.size(); ++i) {
                if (!first) out << ",";
                first = false;
                out << "{\"height\":" << i << "}";
            }
            out << "]}";
            res.set_content(out.str(), "application/json");
        });
    }
};

}  // namespace caesar
