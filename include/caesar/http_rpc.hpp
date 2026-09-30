#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <httplib.h>

#include <caesar/node.hpp>
#include <caesar/persistent_wallet.hpp>
#include <caesar/transaction_signature.hpp>
#include <caesar/utxo.hpp>
#include <caesar/witness.hpp>

namespace caesar {

inline const char* WALLET_HTML = R"HTML(<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Caesar CZR Wallet</title>
<style>
body{font-family:monospace;background:#0f1115;color:#e8e8e8;padding:12px;margin:0}
h1{color:#f0c040;font-size:20px;margin:0 0 10px}
h2{color:#f0c040;font-size:15px;margin:0 0 8px}
.card{background:#1a1d24;padding:12px;margin:8px 0;border-radius:8px;border:1px solid #252a33}
.val{color:#5fdc7a;font-weight:bold;word-break:break-all}
.row{display:flex;justify-content:space-between;padding:3px 0;font-size:13px}
button{background:#f0c040;color:#000;border:none;padding:10px 14px;font-size:14px;cursor:pointer;margin:3px 3px 3px 0;border-radius:6px;font-weight:bold}
button:active{background:#d0a020}
input{width:100%;padding:8px;margin:4px 0;background:#000;color:#fff;border:1px solid #333;border-radius:4px;font-family:monospace;font-size:13px;box-sizing:border-box}
pre{background:#000;padding:8px;border-radius:4px;font-size:11px;max-height:180px;overflow-y:auto;white-space:pre-wrap;word-break:break-all;margin:0}
.bal{font-size:22px;color:#5fdc7a;font-weight:bold}
</style>
</head>
<body>
<h1>⚡ Caesar CZR Wallet</h1>

<div class="card">
<h2>Node</h2>
<div class="row"><span>Height</span><span class="val" id="height">-</span></div>
<div class="row"><span>Peers</span><span class="val" id="peers">-</span></div>
<div class="row"><span>Mempool</span><span class="val" id="mempool">-</span></div>
<div class="row"><span>Uptime</span><span class="val" id="uptime">-</span></div>
</div>

<div class="card">
<h2>My Wallet</h2>
<div class="row"><span>Balance</span><span class="bal" id="balance">-</span></div>
<div style="font-size:11px;color:#888;margin-top:6px">Address</div>
<div class="val" id="addr" style="font-size:10px">-</div>
<button onclick="doMine()">⛏️ Mine</button>
<button onclick="doRefresh()">🔄 Refresh</button>
</div>

<div class="card">
<h2>Send CZR</h2>
<input id="recipient" placeholder="recipient address (CZ1...)">
<input id="amount" placeholder="amount" type="number">
<button onclick="doSend()">📤 Send</button>
</div>

<div class="card">
<h2>Log</h2>
<pre id="log">Ready.</pre>
</div>

<script>
function log(m){
  var t=new Date().toLocaleTimeString();
  document.getElementById('log').textContent=t+' | '+m+'\n'+document.getElementById('log').textContent;
}
async function api(path,opts){
  var r=await fetch(path,opts);
  return await r.json();
}
async function refreshStatus(){
  try{
    var d=await api('/api/status');
    document.getElementById('height').textContent=d.height;
    document.getElementById('peers').textContent=d.peers;
    document.getElementById('mempool').textContent=d.mempool;
    document.getElementById('uptime').textContent=d.uptime+'s';
  }catch(e){}
}
async function refreshWallet(){
  try{
    var w=await api('/api/wallet');
    document.getElementById('addr').textContent=w.address||'-';
    var b=await api('/api/balance');
    document.getElementById('balance').textContent=b.balance+' CZR';
  }catch(e){log('Refresh err: '+e.message);}
}
async function doMine(){
  log('Mining...');
  try{
    var d=await api('/api/mine_default',{method:'POST'});
    if(d.error){log('Err: '+d.error);return;}
    log('Mined! Height: '+d.height);
    refreshStatus();refreshWallet();
  }catch(e){log('Mine err: '+e.message);}
}
async function doSend(){
  var r=document.getElementById('recipient').value.trim();
  var a=document.getElementById('amount').value.trim();
  if(!r||!a){log('Fill recipient and amount');return;}
  log('Sending '+a+' to '+r.substring(0,20)+'...');
  try{
    var d=await api('/api/send',{method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify({recipient:r,amount:parseInt(a)})});
    if(d.error){log('Send err: '+d.error);return;}
    log('Sent! TxID: '+(d.txid||'').substring(0,16));
    refreshWallet();
  }catch(e){log('Send err: '+e.message);}
}
function doRefresh(){refreshStatus();refreshWallet();log('Refreshed');}
refreshStatus();refreshWallet();
setInterval(refreshStatus,3000);
</script>
</body>
</html>)HTML";

class HttpRpcServer {
   public:
    HttpRpcServer(CaesarNode& node, std::uint16_t port,
                  const std::filesystem::path& data_dir)
        : node_(node),
          port_(port),
          started_at_(std::chrono::steady_clock::now()),
          wallet_path_(data_dir / "wallet.pem") {
        try {
            wallet_ = std::make_unique<PersistentWallet>(wallet_path_);
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

    bool running() const { return server_.is_running(); }

   private:
    CaesarNode& node_;
    std::uint16_t port_;
    httplib::Server server_;
    std::thread thread_;
    std::chrono::steady_clock::time_point started_at_;
    std::filesystem::path wallet_path_;
    std::unique_ptr<PersistentWallet> wallet_;

    struct WalletUtxo {
        Hash256 txid;
        std::uint32_t index;
        std::uint64_t amount;
    };

    std::string my_address() const {
        if (!wallet_) return "";
        try {
            return wallet_->address();
        } catch (...) {
            return "";
        }
    }

    std::vector<WalletUtxo> collect_my_utxos() const {
        const std::string addr = my_address();
        std::vector<WalletUtxo> result;
        if (addr.empty()) return result;

        std::unordered_set<OutPoint, OutPointHasher> spent;
        std::unordered_map<OutPoint, WalletUtxo, OutPointHasher> unspent;

        auto process = [&](const std::vector<Transaction>& txs) {
            for (const auto& tx : txs) {
                for (const auto& in : tx.inputs)
                    spent.insert(OutPoint{in.previous_txid, in.output_index});

                const Hash256 txid = tx.txid();
                for (std::size_t i = 0; i < tx.outputs.size(); ++i) {
                    if (tx.outputs[i].recipient == addr) {
                        OutPoint op{txid, static_cast<std::uint32_t>(i)};
                        unspent[op] = WalletUtxo{txid, static_cast<std::uint32_t>(i),
                                                 tx.outputs[i].amount};
                    }
                }
            }
        };

        for (const auto& block : node_.chain()) process(block.transactions);
        process(node_.mempool_transactions());

        for (auto& entry : unspent) {
            if (!spent.contains(entry.first)) result.push_back(entry.second);
        }
        return result;
    }

    std::uint64_t compute_balance() const {
        std::uint64_t total = 0;
        for (const auto& u : collect_my_utxos()) total += u.amount;
        return total;
    }

    void setup_routes() {
        server_.Get("/", [](const httplib::Request&, httplib::Response& res) {
            res.set_content(WALLET_HTML, "text/html; charset=utf-8");
        });

        server_.Get("/api/status", [this](const httplib::Request&, httplib::Response& res) {
            const auto now = std::chrono::steady_clock::now();
            const auto uptime =
                std::chrono::duration_cast<std::chrono::seconds>(now - started_at_).count();
            std::ostringstream out;
            out << "{\"height\":" << node_.height()
                << ",\"peers\":" << node_.peer_count()
                << ",\"mempool\":" << node_.mempool_size()
                << ",\"uptime\":" << uptime << "}";
            res.set_content(out.str(), "application/json");
        });

        server_.Get("/api/wallet", [this](const httplib::Request&, httplib::Response& res) {
            std::string pk, addr;
            try {
                if (wallet_) {
                    pk = wallet_->public_key();
                    addr = wallet_->address();
                }
            } catch (...) {}
            res.set_content("{\"public_key\":\"" + pk + "\",\"address\":\"" + addr + "\"}",
                            "application/json");
        });

        server_.Get("/api/balance", [this](const httplib::Request&, httplib::Response& res) {
            std::ostringstream out;
            out << "{\"balance\":" << compute_balance() << "}";
            res.set_content(out.str(), "application/json");
        });

        server_.Get("/api/history", [this](const httplib::Request&, httplib::Response& res) {
            const std::string addr = my_address();
            std::ostringstream out;
            out << "{\"transactions\":[";
            bool first = true;
            int count = 0;
            auto chain = node_.chain();
            for (std::size_t h = chain.size(); h-- > 0 && count < 20;) {
                for (const auto& tx : chain[h].transactions) {
                    bool involves = false;
                    for (const auto& o : tx.outputs)
                        if (o.recipient == addr) involves = true;
                    if (!involves) continue;
                    if (!first) out << ",";
                    first = false;
                    out << "{\"block\":" << h << ",\"txid\":\""
                        << hash_to_hex(tx.txid()).substr(0, 16) << "\"}";
                    ++count;
                }
            }
            out << "]}";
            res.set_content(out.str(), "application/json");
        });

        server_.Post("/api/mine_default", [this](const httplib::Request&,
                                                  httplib::Response& res) {
            try {
                std::string recipient = my_address();
                if (recipient.empty()) recipient = "CAESAR_MINER_CZR1";
                node_.mine_one_block(recipient, 1000000);
                std::ostringstream out;
                out << "{\"status\":\"ok\",\"height\":" << node_.height() << "}";
                res.set_content(out.str(), "application/json");
            } catch (const std::exception& e) {
                res.status = 500;
                res.set_content(std::string("{\"error\":\"") + e.what() + "\"}",
                                "application/json");
            }
        });

        server_.Post("/api/send", [this](const httplib::Request& req, httplib::Response& res) {
            try {
                if (!wallet_) throw std::runtime_error("wallet unavailable");

                // Minimal JSON parse
                auto get_field = [&](const std::string& key) -> std::string {
                    auto pos = req.body.find("\"" + key + "\"");
                    if (pos == std::string::npos) return "";
                    auto colon = req.body.find(':', pos);
                    if (colon == std::string::npos) return "";
                    auto start = req.body.find_first_of("\"0123456789", colon);
                    if (start == std::string::npos) return "";
                    if (req.body[start] == '"') {
                        auto end = req.body.find('"', start + 1);
                        if (end == std::string::npos) return "";
                        return req.body.substr(start + 1, end - start - 1);
                    }
                    auto end = req.body.find_first_of(",}", start);
                    if (end == std::string::npos) return "";
                    return req.body.substr(start, end - start);
                };

                std::string recipient = get_field("recipient");
                std::string amount_s = get_field("amount");
                if (recipient.empty() || amount_s.empty())
                    throw std::runtime_error("missing recipient or amount");

                std::uint64_t amount = std::stoull(amount_s);
                if (amount == 0) throw std::runtime_error("amount must be > 0");
                if (!is_valid_address(recipient))
                    throw std::runtime_error("invalid recipient address");

                auto utxos = collect_my_utxos();

                std::uint64_t total = 0;
                for (const auto& u : utxos) total += u.amount;
                if (total < amount)
                    throw std::runtime_error("insufficient balance");

                Transaction tx;
                tx.version = 1;

                std::uint64_t gathered = 0;
                for (const auto& u : utxos) {
                    TransactionInput in;
                    in.previous_txid = u.txid;
                    in.output_index = u.index;
                    tx.inputs.push_back(in);
                    gathered += u.amount;
                    if (gathered >= amount) break;
                }

                TransactionOutput out;
                out.amount = amount;
                out.recipient = recipient;
                tx.outputs.push_back(out);

                const std::uint64_t change = gathered - amount;
                if (change > 0) {
                    TransactionOutput ch;
                    ch.amount = change;
                    ch.recipient = wallet_->address();
                    tx.outputs.push_back(ch);
                }

                const std::string pk = wallet_->public_key();
                for (std::size_t i = 0; i < tx.inputs.size(); ++i) {
                    auto sig = sign_transaction_input(tx, i, wallet_->private_key());
                    TransactionWitness w;
                    w.public_key = pk;
                    w.signature = sig;
                    tx.witness.inputs.push_back(w);
                }

                if (!node_.submit_transaction(tx))
                    throw std::runtime_error("mempool rejected transaction");

                res.set_content("{\"status\":\"ok\",\"txid\":\"" + hash_to_hex(tx.txid()) + "\"}",
                                "application/json");
            } catch (const std::exception& e) {
                res.status = 500;
                res.set_content(std::string("{\"error\":\"") + e.what() + "\"}",
                                "application/json");
            }
        });
    }
};

}  // namespace caesar
