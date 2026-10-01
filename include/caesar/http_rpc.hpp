#pragma once
#include <iostream>
#include <atomic>
#include <fstream>
#include <ctime>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <openssl/rand.h>

#include <httplib.h>

#include <caesar/node.hpp>
#include <caesar/persistent_wallet.hpp>
#include <caesar/wallet_auth.hpp>
#include <caesar/bip39.hpp>
#include <caesar/key_encoding.hpp>
#include <qrcodegen.hpp>
#include <caesar/transaction_signature.hpp>
#include <caesar/utxo.hpp>
#include <caesar/witness.hpp>

namespace caesar {

struct PoolWorkerInfo {
    std::string address;
    std::string worker_name;
    std::uint64_t shares = 0;
    std::uint64_t shares_pending = 0;
    std::uint64_t last_seen = 0;
};

struct PoolPayoutRecord {
    std::uint64_t timestamp = 0;
    std::uint64_t block_height = 0;
    std::string txid;
    std::uint64_t total_amount = 0;
    std::uint64_t fee_amount = 0;
    std::uint64_t recipient_count = 0;
};

struct PoolJobState {
    bool active = false;
    std::uint64_t job_id = 0;
    std::string header_hex;
    std::uint32_t share_difficulty = 0;
    std::uint32_t block_difficulty = 0;
    std::uint64_t height = 0;
    Block candidate;
    std::string miner_address;
};

inline std::mutex g_pool_mutex;
inline std::map<std::string, PoolWorkerInfo> g_pool_workers;
inline PoolJobState g_pool_current_job;
inline std::uint64_t g_pool_total_shares = 0;
inline std::uint64_t g_pool_total_blocks = 0;
inline std::uint64_t g_pool_job_counter = 0;
inline double g_pool_fee_percent = 2.0;
inline std::vector<PoolPayoutRecord> g_pool_payouts;

struct PoolPayoutResult {
    bool ok = false;
    std::string error;
    std::string txid;
    std::uint64_t distributed = 0;
    std::uint64_t fee = 0;
    std::size_t recipients = 0;
};

inline std::atomic<bool>          g_pool_auto_enabled{false};
inline std::atomic<std::uint32_t> g_pool_auto_blocks{10};
inline std::atomic<std::uint64_t> g_pool_last_payout_height{0};

inline std::string pool_bytes_to_hex(const std::vector<std::uint8_t>& data) {
    static const char* hex = "0123456789abcdef";
    std::string out;
    out.reserve(data.size() * 2);
    for (auto b : data) {
        out.push_back(hex[(b >> 4) & 0xF]);
        out.push_back(hex[b & 0xF]);
    }
    return out;
}

inline std::vector<std::uint8_t> pool_hex_to_bytes(const std::string& h) {
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

inline std::atomic<bool> g_pool_state_loaded{false};

inline void pool_save_state_unlocked(const std::filesystem::path& p) {
    std::ofstream f(p, std::ios::trunc);
    if (!f) return;
    f << "total_shares=" << g_pool_total_shares << "\n";
    f << "total_blocks=" << g_pool_total_blocks << "\n";
    f << "job_counter=" << g_pool_job_counter << "\n";
    f << "fee_percent=" << g_pool_fee_percent << "\n";
    f << "auto_enabled=" << (g_pool_auto_enabled.load() ? "1" : "0") << "\n";
    f << "auto_blocks=" << g_pool_auto_blocks.load() << "\n";
    f << "last_payout_height=" << g_pool_last_payout_height.load() << "\n";
    for (const auto& kv : g_pool_workers) {
        const auto& w = kv.second;
        f << "worker=" << w.address << "|" << w.worker_name << "|"
          << w.shares << "|" << w.shares_pending << "|" << w.last_seen << "\n";
    }
    for (const auto& r : g_pool_payouts) {
        f << "payout=" << r.timestamp << "|" << r.block_height << "|"
          << r.txid << "|" << r.total_amount << "|" << r.fee_amount << "|"
          << r.recipient_count << "\n";
    }
    f.flush();
}

inline void pool_load_state_unlocked(const std::filesystem::path& p) {
    std::ifstream f(p);
    if (!f) return;
    g_pool_workers.clear();
    g_pool_payouts.clear();
    auto split = [](const std::string& s, char d) {
        std::vector<std::string> parts;
        std::string cur;
        for (char ch : s) {
            if (ch == d) { parts.push_back(cur); cur.clear(); }
            else cur += ch;
        }
        parts.push_back(cur);
        return parts;
    };
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        try {
            if (key == "total_shares") g_pool_total_shares = std::stoull(val);
            else if (key == "total_blocks") g_pool_total_blocks = std::stoull(val);
            else if (key == "job_counter") g_pool_job_counter = std::stoull(val);
            else if (key == "fee_percent") g_pool_fee_percent = std::stod(val);
            else if (key == "worker") {
                auto q = split(val, '|');
                if (q.size() >= 5) {
                    PoolWorkerInfo w;
                    w.address = q[0];
                    w.worker_name = q[1];
                    w.shares = std::stoull(q[2]);
                    w.shares_pending = std::stoull(q[3]);
                    w.last_seen = std::stoull(q[4]);
                    g_pool_workers[w.address] = w;
                }
            } else if (key == "payout") {
                auto q = split(val, '|');
                if (q.size() >= 6) {
                    PoolPayoutRecord r;
                    r.timestamp = std::stoull(q[0]);
                    r.block_height = std::stoull(q[1]);
                    r.txid = q[2];
                    r.total_amount = std::stoull(q[3]);
                    r.fee_amount = std::stoull(q[4]);
                    r.recipient_count = std::stoull(q[5]);
                    g_pool_payouts.push_back(r);
                }
            }
        } catch (...) {}
    }
}



inline const char* WALLET_HTML = R"HTML(<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta http-equiv="Cache-Control" content="no-cache, no-store, must-revalidate">
<meta http-equiv="Pragma" content="no-cache">
<meta http-equiv="Expires" content="0">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="theme-color" content="#f0c040">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-title" content="Caesar">
<link rel="manifest" href="/manifest.json">
<link rel="icon" href="/icon-192.svg" type="image/svg+xml">
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

<!-- PIN Lock Screen -->
<div id="lockScreen" style="position:fixed;top:0;left:0;width:100%;height:100%;background:#0f1115;z-index:9999;display:flex;flex-direction:column;justify-content:center;align-items:center;padding:20px;box-sizing:border-box">
  <h1 style="color:#f0c040;font-size:26px;margin-bottom:24px;font-family:monospace">&#9889; Caesar CZR</h1>
  <div id="lockTitle" style="color:#e8e8e8;font-size:15px;margin-bottom:14px;font-family:monospace">Enter your PIN</div>
  <input id="pinInput" type="tel" inputmode="numeric" pattern="[0-9]*" placeholder="PIN" autocomplete="off" style="background:#000;color:#fff;border:2px solid #f0c040;border-radius:8px;padding:14px;font-size:24px;text-align:center;width:220px;letter-spacing:8px;margin:8px 0;font-family:monospace" maxlength="12">
  <button id="unlockBtn" type="button" style="background:#f0c040;color:#000;border:none;padding:14px 50px;font-size:16px;font-weight:bold;border-radius:8px;margin-top:14px;cursor:pointer;font-family:monospace">UNLOCK</button>
  <div id="lockMsg" style="color:#ff6b6b;font-size:13px;margin-top:14px;min-height:20px;text-align:center;max-width:280px;font-family:monospace"></div>
</div>

<div id="walletContent" style="display:none">
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

<div id="qrWrap" style="text-align:center;margin:12px 0 6px 0;display:none">
  <img id="qrImg" src="/api/wallet/qr.svg" width="300" height="300" style="background:#fff;padding:10px;border-radius:8px" alt="QR">
  <div style="font-size:10px;color:#888;margin-top:6px">Scan to receive CZR</div>
</div>
<div style="font-size:11px;color:#888;margin-top:6px">Address</div>
<div class="val" id="addr" style="font-size:10px">-</div>
<button onclick="doMine()">⛏️ Mine</button>
<button onclick="doRefresh()">🔄 Refresh</button>
</div>

<div class="card">
<h2>Send CZR</h2>
<input id="recipient" placeholder="recipient address (CZ1...)">
<input id="amount" placeholder="amount" type="number">
<button onclick="confirmSend()">📤 Send</button>
</div>

<div class="card">
<h2>Sign / Verify</h2>
<div style="font-size:11px;color:#888;margin-bottom:6px">Sign a message with your wallet key</div>
<textarea id="signMsg" placeholder="message" style="background:#000;color:#fff;border:1px solid #333;border-radius:4px;padding:8px;width:100%;height:50px;box-sizing:border-box;font-family:monospace;font-size:12px"></textarea>
<button onclick="doSign()" style="margin-top:6px">Sign</button>
<div style="font-size:11px;color:#888;margin-top:10px">Signature</div>
<div id="sigOutput" style="background:#000;color:#5fdc7a;border:1px solid #333;border-radius:4px;padding:8px;min-height:40px;font-family:monospace;font-size:11px;word-break:break-all;box-sizing:border-box">-</div>
<div style="font-size:11px;color:#888;margin-top:10px">Verify</div>
<textarea id="verifyMsg" placeholder="message" style="background:#000;color:#fff;border:1px solid #333;border-radius:4px;padding:8px;width:100%;height:36px;box-sizing:border-box;font-family:monospace;font-size:12px;margin-bottom:4px"></textarea>
<textarea id="verifySig" placeholder="signature (hex)" style="background:#000;color:#fff;border:1px solid #333;border-radius:4px;padding:8px;width:100%;height:36px;box-sizing:border-box;font-family:monospace;font-size:12px"></textarea>
<button onclick="doVerify()" style="margin-top:6px">Verify</button>
<div id="verifyOutput" style="margin-top:8px;font-size:12px;font-family:monospace">-</div>
</div>

<div class="card">
<h2>Transactions</h2>
<div id="txList" style="font-size:12px;font-family:monospace;color:#5fdc7a">Loading...</div>
</div>

<div class="card">
<h2>Address Book</h2>
<div style="font-size:11px;color:#888;margin-bottom:6px">Save frequently used addresses</div>
<div style="margin-bottom:8px">
  <input id="abName" placeholder="name" style="width:100%;box-sizing:border-box;background:#000;color:#fff;border:1px solid #333;border-radius:4px;padding:8px;font-family:monospace;font-size:12px;margin-bottom:6px">
  <div style="display:flex;gap:6px">
    <input id="abAddr" placeholder="CZ1..." style="flex:1;min-width:0;background:#000;color:#fff;border:1px solid #333;border-radius:4px;padding:8px;font-family:monospace;font-size:11px">
    <button onclick="addContact()" style="padding:8px 16px;font-size:16px;font-weight:bold;background:#f0c040;color:#000;border:none;border-radius:4px">+</button>
  </div>
</div>
<div id="abList" style="font-size:12px;font-family:monospace"></div>
</div>

<div class="card">
<h2>Change PIN</h2>
<div style="font-size:11px;color:#888;margin-bottom:6px">Replace your unlock PIN</div>
<input id="oldPin" type="tel" placeholder="old PIN" style="width:100%;box-sizing:border-box;background:#000;color:#fff;border:1px solid #333;border-radius:4px;padding:8px;font-family:monospace;font-size:14px;letter-spacing:4px;margin-bottom:6px">
<input id="newPin" type="tel" placeholder="new PIN" style="width:100%;box-sizing:border-box;background:#000;color:#fff;border:1px solid #333;border-radius:4px;padding:8px;font-family:monospace;font-size:14px;letter-spacing:4px;margin-bottom:6px">
<button onclick="doChangePin()" style="padding:8px 16px">Change PIN</button>
<div id="cpResult" style="margin-top:8px;font-family:monospace;font-size:12px">-</div>
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
async function refreshHistory(){
  try{
    var d=await api('/api/wallet/history');
    var el=document.getElementById('txList');
    if(!d.transactions||d.transactions.length===0){el.textContent='No transactions yet.';return;}
    var html='';
    d.transactions.slice(0,20).forEach(function(t){
      var when=new Date(t.timestamp*1000).toISOString().replace('T',' ').substring(0,19);
      var amt=(t.amount/100000000).toFixed(2);
      html+='<div style="display:flex;justify-content:space-between;padding:4px 0;border-bottom:1px solid #1a1a1a">'
           +'<span style="color:#888">#'+t.block+'</span>'
           +'<span style="color:#ccc">'+when+'</span>'
           +'<span style="color:#5fdc7a">+'+amt+' CZR</span>'
           +'</div>';
    });
    el.innerHTML=html;
  }catch(e){document.getElementById('txList').textContent='Error: '+e.message;}
}

function loadContacts(){
  try{return JSON.parse(localStorage.getItem('czr_contacts')||'[]');}catch(e){return [];}
}
function saveContacts(list){
  localStorage.setItem('czr_contacts',JSON.stringify(list));
}
function renderContacts(){
  var el=document.getElementById('abList');
  if(!el)return;
  var list=loadContacts();
  if(list.length===0){el.innerHTML='<div style="color:#666">no contacts saved</div>';return;}
  var html='';
  list.forEach(function(c,i){
    html+='<div style="display:flex;justify-content:space-between;padding:4px 0;border-bottom:1px solid #1a1a1a">'
         +'<span style="color:#f0c040">'+c.name+'</span>'
         +'<span style="color:#888">'+c.addr.substring(0,20)+'...</span>'
         +'<span><button onclick="useContact('+i+')" style="font-size:10px;padding:2px 6px">use</button>'
         +' <button onclick="delContact('+i+')" style="font-size:10px;padding:2px 6px">x</button></span>'
         +'</div>';
  });
  el.innerHTML=html;
}
function addContact(){
  var n=document.getElementById('abName').value.trim();
  var a=document.getElementById('abAddr').value.trim();
  if(!n||!a){log('name and address required');return;}
  var list=loadContacts();
  list.push({name:n,addr:a});
  saveContacts(list);
  document.getElementById('abName').value='';
  document.getElementById('abAddr').value='';
  renderContacts();
  log('Contact saved: '+n);
}
function delContact(i){
  var list=loadContacts();
  list.splice(i,1);
  saveContacts(list);
  renderContacts();
}
function useContact(i){
  var list=loadContacts();
  var c=list[i];
  if(!c)return;
  var r=document.getElementById('recipient');
  if(r){r.value=c.addr;log('Recipient set: '+c.name);}
}

async function refreshWallet(){
  try{
    var w=await api('/api/wallet');
    document.getElementById('addr').textContent=w.address||'-';
    document.getElementById('qrWrap').style.display='block';
    var b=await api('/api/balance');
    document.getElementById('balance').textContent=b.balance+' CZR';
    refreshHistory();
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
async function doChangePin(){
  var op=document.getElementById('oldPin').value.trim();
  var np=document.getElementById('newPin').value.trim();
  var el=document.getElementById('cpResult');
  if(op.length<4||np.length<4){el.style.color='#ff6b6b';el.textContent='Both PINs must be 4+ chars';return;}
  el.style.color='#888';el.textContent='Changing...';
  try{
    var r=await fetch('/api/auth/change-pin',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({old_pin:op,new_pin:np})});
    var d=await r.json();
    if(d.status==='ok'){el.style.color='#5fdc7a';el.textContent='PIN changed. Reload and login with new PIN.';log('PIN changed');}
    else{el.style.color='#ff6b6b';el.textContent=d.error||'failed';}
  }catch(e){el.style.color='#ff6b6b';el.textContent='Error: '+e.message;}
}
function confirmSend(){
  var r=document.getElementById('recipient').value.trim();
  var a=document.getElementById('amount').value.trim();
  if(!r||!a){log('Fill recipient and amount');return;}
  var amt=parseInt(a);
  if(isNaN(amt)||amt<=0){log('Invalid amount');return;}
  document.getElementById('modalTo').textContent=r;
  document.getElementById('modalAmt').textContent=amt+' CZR';
  document.getElementById('sendModal').style.display='flex';
}
function closeSendModal(){
  document.getElementById('sendModal').style.display='none';
}
async function doSendConfirmed(){
  closeSendModal();
  await doSend();
}

async function doSign(){
  var msg=document.getElementById('signMsg').value;
  if(!msg){log('Enter a message');return;}
  try{
    var r=await fetch('/api/wallet/sign',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({message:msg})});
    var d=await r.json();
    if(d.error){log('Sign error: '+d.error);return;}
    document.getElementById('sigOutput').textContent=d.signature||'-';
    document.getElementById('verifyMsg').value=msg;
    document.getElementById('verifySig').value=d.signature||'';
    log('Signed');
  }catch(e){log('Sign err: '+e.message);}
}
async function doVerify(){
  var msg=document.getElementById('verifyMsg').value;
  var sig=document.getElementById('verifySig').value.trim();
  if(!msg||!sig){log('Fill message and signature');return;}
  try{
    var r=await fetch('/api/wallet/verify',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({message:msg,signature:sig})});
    var d=await r.json();
    var el=document.getElementById('verifyOutput');
    if(d.valid===true){el.style.color='#5fdc7a';el.textContent='VALID';}
    else{el.style.color='#ff6b6b';el.textContent='INVALID';}
  }catch(e){log('Verify err: '+e.message);}
}
refreshStatus();refreshWallet();
setInterval(refreshStatus,3000);

async function checkAuth(){
  try{
    var r = await fetch('/api/auth/status', {cache:'no-store'});
    var d = await r.json();
    var lock = document.getElementById('lockScreen');
    var wallet = document.getElementById('walletContent');
    if (d.unlocked) {
      lock.style.display = 'none';
      wallet.style.display = 'block';
      refreshStatus();
      refreshWallet();
    } else {
      lock.style.display = 'flex';
      wallet.style.display = 'none';
    }
  } catch(e) {
    document.getElementById('lockScreen').style.display = 'flex';
    document.getElementById('walletContent').style.display = 'none';
  }
}

checkAuth();
</script>
<script>
function bindUnlockBtn() {
    var btn = document.getElementById('unlockBtn');
    var inp = document.getElementById('pinInput');
    var msg = document.getElementById('lockMsg');
    if (!btn || !inp || !msg) return;

    function doUnlock(e) {
        if (e) { e.preventDefault(); e.stopPropagation(); }
        var pin = inp.value.trim();
        if (pin.length < 4) {
            msg.style.color = '#ff6b6b';
            msg.textContent = 'PIN must be at least 4 characters';
            return false;
        }
        msg.style.color = '#5fdc7a';
        msg.textContent = 'Checking...';

        fetch('/api/auth/unlock', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({pin: pin})
        })
        .then(function(r) { return r.json(); })
        .then(function(d) {
            if (d.unlocked) {
                msg.textContent = 'Success!';
                setTimeout(function() {
                    document.getElementById('lockScreen').style.display = 'none';
                    document.getElementById('walletContent').style.display = 'block';
                    refreshStatus();
                    refreshWallet();
                }, 300);
            } else {
                msg.style.color = '#ff6b6b';
                msg.textContent = d.error || 'Wrong PIN';
            }
        })
        .catch(function(e) {
            msg.style.color = '#ff6b6b';
            msg.textContent = 'Error: ' + e.message;
        });
        return false;
    }

    btn.addEventListener('click', doUnlock);
    btn.addEventListener('touchend', doUnlock);
}

if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', bindUnlockBtn);
} else {
    bindUnlockBtn();
}
</script>
</div>
<div id="sendModal" style="position:fixed;inset:0;background:rgba(0,0,0,.85);display:none;z-index:20000;align-items:center;justify-content:center;padding:20px">
  <div style="background:#1a1d23;border:1px solid #444;border-radius:12px;padding:24px;max-width:400px;width:100%;font-family:monospace">
    <h3 style="color:#f0c040;margin:0 0 20px 0;font-size:16px">Confirm Send</h3>
    <div style="font-size:11px;color:#888;margin-bottom:4px">To</div>
    <div id="modalTo" style="font-size:11px;color:#5fdc7a;word-break:break-all;margin-bottom:14px">-</div>
    <div style="font-size:11px;color:#888;margin-bottom:4px">Amount</div>
    <div id="modalAmt" style="font-size:16px;color:#fff;margin-bottom:20px;font-weight:bold">-</div>
    <div style="display:flex;gap:10px">
      <button onclick="closeSendModal()" style="flex:1;background:#333;color:#fff;border:none;padding:12px;border-radius:6px;font-family:monospace;font-size:14px;cursor:pointer">Cancel</button>
      <button onclick="doSendConfirmed()" style="flex:1;background:#f0c040;color:#000;border:none;padding:12px;border-radius:6px;font-weight:bold;font-family:monospace;font-size:14px;cursor:pointer">Confirm</button>
    </div>
  </div>
</div>

</body>
</html>)HTML";

class HttpRpcServer {
   public:
    HttpRpcServer(CaesarNode& node, std::uint16_t port,
                  const std::filesystem::path& data_dir)
        : node_(node),
          port_(port),
          started_at_(std::chrono::steady_clock::now()),
          wallet_path_(data_dir / "wallet.pem"),
          pin_path_(data_dir / "pin.hash"),
          session_file_(data_dir / "session.txt") {
        std::ifstream sf(session_file_);
        if (sf) std::getline(sf, session_token_);

        // Record whether the wallet pre-existed BEFORE PersistentWallet
        // may create a fresh one.
        {
            std::error_code ec;
            const bool exists = std::filesystem::exists(wallet_path_, ec);
            wallet_preexisting_ = (!ec && exists &&
                std::filesystem::file_size(wallet_path_, ec) > 0 && !ec);
        }

        wallet_ = std::make_unique<PersistentWallet>(wallet_path_);
        setup_routes();
    }

    HttpRpcServer(const HttpRpcServer&) = delete;
    HttpRpcServer& operator=(const HttpRpcServer&) = delete;

    void start() {
        thread_ = std::thread([this]() { server_.listen("127.0.0.1", port_); });
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

    PoolPayoutResult pool_execute_payout() {
        PoolPayoutResult r;
        try {
            if (!wallet_) { r.error = "wallet unavailable"; return r; }
            const std::string pool_addr = wallet_->address();
            if (pool_addr.empty()) { r.error = "pool address unavailable"; return r; }
            std::lock_guard<std::mutex> lock(g_pool_mutex);
            std::uint64_t total_pending = 0;
            for (const auto& kv : g_pool_workers)
                if (kv.second.shares_pending > 0) total_pending += kv.second.shares_pending;
            if (total_pending == 0) { r.error = "no pending shares"; return r; }
            auto utxos = collect_my_utxos();
            std::uint64_t total_utxo = 0;
            for (const auto& u : utxos) total_utxo += u.amount;
            if (total_utxo < 10000) { r.error = "pool balance too small"; return r; }
            const std::uint64_t RESERVE = 1000;
            const std::uint64_t distributable = total_utxo - RESERVE;
            const double fee_frac = g_pool_fee_percent / 100.0;
            const std::uint64_t fee_amount =
                static_cast<std::uint64_t>(distributable * fee_frac);
            const std::uint64_t worker_pool = distributable - fee_amount;
            Transaction tx;
            tx.version = 1;
            std::uint64_t assigned = 0;
            for (const auto& kv : g_pool_workers) {
                if (kv.second.shares_pending == 0) continue;
                std::uint64_t amount =
                    (worker_pool * kv.second.shares_pending) / total_pending;
                if (amount == 0) continue;
                TransactionOutput o;
                o.amount = amount;
                o.recipient = kv.second.address;
                tx.outputs.push_back(o);
                assigned += amount;
            }
            if (tx.outputs.empty()) { r.error = "no outputs above dust"; return r; }
            std::uint64_t gathered = 0;
            for (const auto& u : utxos) {
                TransactionInput in;
                in.previous_txid = u.txid;
                in.output_index = u.index;
                tx.inputs.push_back(in);
                gathered += u.amount;
                if (gathered >= assigned + 5000) break;
            }
            if (gathered < assigned) { r.error = "insufficient utxos"; return r; }
            const std::uint64_t change = gathered - assigned;
            if (change > 0) {
                TransactionOutput ch;
                ch.amount = change;
                ch.recipient = pool_addr;
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
            if (!node_.submit_transaction(tx)) { r.error = "mempool rejected"; return r; }
            PoolPayoutRecord rec;
            rec.timestamp = static_cast<std::uint64_t>(::time(nullptr));
            rec.block_height = node_.height();
            rec.txid = hash_to_hex(tx.txid());
            rec.total_amount = assigned;
            rec.fee_amount = fee_amount;
            rec.recipient_count = tx.outputs.size();
            g_pool_payouts.push_back(rec);
            for (auto& kv : g_pool_workers) kv.second.shares_pending = 0;
            pool_save_state_unlocked(wallet_path_.parent_path() / "pool_state.txt");
            r.ok = true;
            r.txid = rec.txid;
            r.distributed = assigned;
            r.fee = fee_amount;
            r.recipients = tx.outputs.size();
            return r;
        } catch (const std::exception& e) {
            r.error = e.what();
            return r;
        }
    }

    void pool_maybe_auto_payout() {
        if (!g_pool_auto_enabled.load()) return;
        std::size_t h = 0;
        try { h = node_.height(); } catch (...) { return; }
        const std::uint64_t last = g_pool_last_payout_height.load();
        const std::uint32_t blk = g_pool_auto_blocks.load();
        if (blk == 0) return;
        if (static_cast<std::uint64_t>(h) < last + blk) return;
        std::uint64_t pending = 0;
        {
            std::lock_guard<std::mutex> lk(g_pool_mutex);
            for (auto& kv : g_pool_workers) pending += kv.second.shares_pending;
        }
        if (pending == 0) return;
        PoolPayoutResult r = pool_execute_payout();
        if (r.ok) {
            g_pool_last_payout_height.store(static_cast<std::uint64_t>(h));
            pool_save_state_unlocked(wallet_path_.parent_path() / "pool_state.txt");
            std::cout << "[pool-auto] payout at height " << h
                      << " txid=" << r.txid
                      << " distributed=" << r.distributed << std::endl;
        } else {
            std::cout << "[pool-auto] skip: " << r.error << std::endl;
        }
    }

   private:
    CaesarNode& node_;
    std::uint16_t port_;
    httplib::Server server_;
    std::thread thread_;
    std::chrono::steady_clock::time_point started_at_;
    std::filesystem::path wallet_path_;
    std::filesystem::path pin_path_;
    std::unique_ptr<PersistentWallet> wallet_;
    bool wallet_preexisting_ = false;
    bool unlocked_ = false;
    std::string session_token_;
    std::filesystem::path session_file_;
    mutable std::mutex auth_mutex_;
    mutable std::mutex auth_failures_mutex_;
    std::unordered_map<std::string, std::pair<int, std::chrono::steady_clock::time_point>> auth_failures_;

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

    static std::string generate_session_token() {
        unsigned char bytes[32];

        if (RAND_bytes(bytes, sizeof(bytes)) != 1) {
            throw std::runtime_error("secure session token generation failed");
        }

        static constexpr char hex[] = "0123456789abcdef";

        std::string token;
        token.reserve(sizeof(bytes) * 2);

        for (unsigned char byte : bytes) {
            token.push_back(hex[(byte >> 4) & 0x0f]);
            token.push_back(hex[byte & 0x0f]);
        }

        return token;
    }

    bool is_authenticated(const httplib::Request& req) const {
        std::lock_guard<std::mutex> lk(auth_mutex_);
        if (!unlocked_) return false;
        if (session_token_.empty()) return false;
        auto cookie = req.get_header_value("Cookie");
        if (cookie.empty()) return false;
        std::string needle = "caesar_session=" + session_token_;
        auto pos = cookie.find(needle);
        if (pos == std::string::npos) return false;
        size_t after = pos + needle.size();
        if (after < cookie.size()) {
            char c = cookie[after];
            if (c != ';' && c != ' ') return false;
        }
        return true;
    }

    bool is_csrf_safe(const httplib::Request& req) const {
        auto origin = req.get_header_value("Origin");
        if (origin.empty()) return true;
        return origin.find("http://127.0.0.1:") != std::string::npos ||
               origin.find("http://localhost:") != std::string::npos;
    }

    bool check_rate_limit(const std::string& ip) {
        std::lock_guard<std::mutex> lk(auth_failures_mutex_);
        auto now = std::chrono::steady_clock::now();
        auto it = auth_failures_.find(ip);
        if (it == auth_failures_.end()) return true;
        if (it->second.first >= 5) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.second).count();
            if (elapsed < 300) return false;
            auth_failures_.erase(it);
        }
        return true;
    }

    void record_auth_failure(const std::string& ip) {
        std::lock_guard<std::mutex> lk(auth_failures_mutex_);
        auto& entry = auth_failures_[ip];
        entry.first++;
        entry.second = std::chrono::steady_clock::now();
    }

    void clear_auth_failures(const std::string& ip) {
        std::lock_guard<std::mutex> lk(auth_failures_mutex_);
        auth_failures_.erase(ip);
    }

    void setup_routes() {
        // PWA routes
// PIN + auth routes
        server_.Get("/api/auth/status", [this](const httplib::Request& req, httplib::Response& res) {
            bool unlocked = is_authenticated(req);
            std::ostringstream out;
            out << "{\"has_pin\":" << (std::filesystem::exists(pin_path_) ? "true" : "false")
                << ",\"unlocked\":" << (unlocked ? "true" : "false") << "}";
            res.set_header("Cache-Control", "no-store");
            res.set_content(out.str(), "application/json");
        });

        server_.Post("/api/auth/setup", [this](const httplib::Request& req, httplib::Response& res) {
            if (!is_csrf_safe(req)) {
                res.status = 403;
                res.set_content("{\"error\":\"csrf\"}", "application/json");
                return;
            }
            try {
                if (std::filesystem::exists(pin_path_)) {
                    throw std::runtime_error("PIN already set. Use /api/auth/unlock");
                }
                auto pos = req.body.find("\"pin\"");
                if (pos == std::string::npos) throw std::runtime_error("missing pin");
                auto start = req.body.find('"', pos + 5);
                auto end = req.body.find('"', start + 1);
                std::string pin = req.body.substr(start + 1, end - start - 1);
                if (pin.size() < 4) throw std::runtime_error("PIN must be 4+ digits");

                if (!wallet_->is_loaded()) {
                    throw std::runtime_error(
                        "wallet is encrypted on disk and pin.hash is missing; "
                        "manual intervention required");
                }

                std::string mnemonic;

                if (!wallet_preexisting_) {
                    // Fresh wallet on disk: generate a BIP39-backed one.
                    mnemonic = wallet_->create_new_hd_wallet();
                    wallet_->save_encrypted(pin);
                } else {
                    // Legacy wallet (random, no mnemonic) — encrypt it with PIN
                    // so the file is no longer plaintext, but no mnemonic exists.
                    wallet_->save_encrypted(pin);
                }

                caesar::save_pin(pin_path_, pin);

                {
                    std::lock_guard<std::mutex> lk(auth_mutex_);
                    unlocked_ = true;
                    session_token_ = generate_session_token();
                }
                std::ofstream sf(session_file_);
                sf << session_token_;

                std::string addr;
                try { addr = wallet_->address(); } catch (...) {}

                std::ostringstream out;
                out << "{\"status\":\"ok\",\"unlocked\":true";
                if (!mnemonic.empty()) {
                    out << ",\"mnemonic\":\"" << mnemonic << "\"";
                }
                out << ",\"address\":\"" << addr << "\"";
                out << "}";

                res.set_header("Set-Cookie", "caesar_session=" + session_token_ + "; Path=/; Max-Age=604800; SameSite=Strict; HttpOnly");
                res.set_content(out.str(), "application/json");
            } catch (const std::exception& e) {
                res.status = 400;
                res.set_content(std::string("{\"error\":\"") + e.what() + "\"}", "application/json");
            }
        });

        server_.Post("/api/auth/unlock", [this](const httplib::Request& req, httplib::Response& res) {
            if (!is_csrf_safe(req)) {
                res.status = 403;
                res.set_content("{\"error\":\"csrf\"}", "application/json");
                return;
            }
            std::string client_ip = req.remote_addr.empty() ? "local" : req.remote_addr;
            if (!check_rate_limit(client_ip)) {
                res.status = 429;
                res.set_content("{\"error\":\"too many attempts, wait 5 minutes\"}", "application/json");
                return;
            }
            try {
                auto pos = req.body.find("\"pin\"");
                if (pos == std::string::npos) throw std::runtime_error("missing pin");
                auto start = req.body.find('"', pos + 5);
                auto end = req.body.find('"', start + 1);
                std::string pin = req.body.substr(start + 1, end - start - 1);
                if (!std::filesystem::exists(pin_path_)) throw std::runtime_error("no PIN set");
                if (caesar::verify_pin(pin_path_, pin)) {
                    if (!wallet_->is_loaded()) {
                        wallet_->unlock_with_pin(pin);
                    }
                    std::string new_token;
                    {
                        std::lock_guard<std::mutex> lk(auth_mutex_);
                        unlocked_ = true;
                        session_token_ = generate_session_token();
                        new_token = session_token_;
                    }
                    std::ofstream sf(session_file_);
                    sf << new_token;
                    clear_auth_failures(client_ip);
                    res.set_header("Set-Cookie", "caesar_session=" + new_token + "; Path=/; Max-Age=604800; SameSite=Strict; HttpOnly");
                    res.set_content("{\"status\":\"ok\",\"unlocked\":true}", "application/json");
                } else {
                    record_auth_failure(client_ip);
                    res.status = 401;
                    res.set_content("{\"error\":\"invalid PIN\"}", "application/json");
                }
            } catch (const std::exception& e) {
                res.status = 400;
                res.set_content(std::string("{\"error\":\"") + e.what() + "\"}", "application/json");
            }
        });

        // unlock-get removed: PIN-in-URL security issue

        server_.Post("/api/auth/recover", [this](const httplib::Request& req, httplib::Response& res) {
            if (!is_csrf_safe(req)) {
                res.status = 403;
                res.set_content("{\"error\":\"csrf\"}", "application/json");
                return;
            }
            std::string client_ip = req.remote_addr.empty() ? "local" : req.remote_addr;
            if (!check_rate_limit(client_ip)) {
                res.status = 429;
                res.set_content("{\"error\":\"too many attempts, wait 5 minutes\"}", "application/json");
                return;
            }
            try {
                // Parse mnemonic
                auto mpos = req.body.find("\"mnemonic\"");
                if (mpos == std::string::npos) {
                    throw std::runtime_error("missing mnemonic");
                }
                auto mstart = req.body.find('"', mpos + 10);
                auto mend = req.body.find('"', mstart + 1);
                std::string mnemonic = req.body.substr(mstart + 1, mend - mstart - 1);

                // Parse new_pin
                auto ppos = req.body.find("\"new_pin\"");
                if (ppos == std::string::npos) {
                    throw std::runtime_error("missing new_pin");
                }
                auto pstart = req.body.find('"', ppos + 9);
                auto pend = req.body.find('"', pstart + 1);
                std::string new_pin = req.body.substr(pstart + 1, pend - pstart - 1);

                if (new_pin.size() < 4) {
                    throw std::runtime_error("PIN must be 4+ digits");
                }
                if (!caesar::bip39::validate_mnemonic(mnemonic)) {
                    throw std::runtime_error("invalid mnemonic");
                }

                // Replace current wallet with mnemonic-derived one
                wallet_->replace_with_mnemonic(mnemonic);
                wallet_->save_encrypted(new_pin);
                caesar::save_pin(pin_path_, new_pin);

                std::string addr;
                try { addr = wallet_->address(); } catch (...) {}

                {
                    std::lock_guard<std::mutex> lk(auth_mutex_);
                    unlocked_ = true;
                    session_token_ = generate_session_token();
                }
                std::ofstream sf(session_file_);
                sf << session_token_;

                res.set_header("Set-Cookie", "caesar_session=" + session_token_ + "; Path=/; Max-Age=604800; SameSite=Strict; HttpOnly");
                res.set_content("{\"status\":\"ok\",\"unlocked\":true,\"address\":\"" + addr + "\"}", "application/json");
            } catch (const std::exception& e) {
                res.status = 400;
                res.set_content(std::string("{\"error\":\"") + e.what() + "\"}", "application/json");
            }
        });

        server_.Post("/api/auth/encrypt-wallet", [this](const httplib::Request& req, httplib::Response& res) {
            if (!is_csrf_safe(req)) {
                res.status = 403;
                res.set_content("{\"error\":\"csrf\"}", "application/json");
                return;
            }
            try {
                auto pos = req.body.find("\"pin\"");
                if (pos == std::string::npos) {
                    throw std::runtime_error("missing pin");
                }
                auto start = req.body.find('"', pos + 5);
                auto end = req.body.find('"', start + 1);
                std::string pin = req.body.substr(start + 1, end - start - 1);
                if (pin.size() < 4) {
                    throw std::runtime_error("PIN must be 4+ digits");
                }
                if (!std::filesystem::exists(pin_path_)) {
                    throw std::runtime_error("no PIN set; run /api/auth/setup first");
                }
                if (!caesar::verify_pin(pin_path_, pin)) {
                    res.status = 401;
                    res.set_content("{\"error\":\"invalid PIN\"}", "application/json");
                    return;
                }
                if (wallet_->is_file_encrypted()) {
                    res.set_content("{\"status\":\"already_encrypted\"}", "application/json");
                    return;
                }
                if (!wallet_->is_loaded()) {
                    wallet_->unlock_with_pin(pin);
                }
                wallet_->save_encrypted(pin);
                res.set_content("{\"status\":\"ok\",\"encrypted\":true}", "application/json");
            } catch (const std::exception& e) {
                res.status = 400;
                res.set_content(std::string("{\"error\":\"") + e.what() + "\"}", "application/json");
            }
        });

        server_.Get("/api/blocks", [this](const httplib::Request& req, httplib::Response& res) {
            try {
                std::size_t limit = 50;
                auto it = req.params.find("limit");
                if (it != req.params.end()) {
                    try { limit = std::stoul(it->second); } catch (...) {}
                    if (limit == 0 || limit > 500) limit = 50;
                }

                std::vector<Block> chain;
                try {
                    chain = node_.chain();
                } catch (...) {
                    chain.clear();
                }

                std::ostringstream out;
                out << "{\"height\":" << (chain.empty() ? 0 : chain.size() - 1)
                    << ",\"count\":" << chain.size()
                    << ",\"blocks\":[";

                const std::size_t start =
                    chain.size() > limit ? chain.size() - limit : 0;

                for (std::size_t i = start; i < chain.size(); ++i) {
                    if (i > start) out << ",";
                    const auto& blk = chain[i];
                    const auto h = blk.hash();

                    out << "{\"height\":" << blk.header.height;
                    out << ",\"hash\":\"" << hash_to_hex(h) << "\"";
                    out << ",\"prev\":\"" << hash_to_hex(blk.header.previous_hash) << "\"";
                    out << ",\"timestamp\":" << blk.header.timestamp;
                    out << ",\"difficulty\":" << blk.header.difficulty;
                    out << ",\"nonce\":" << blk.header.nonce;
                    out << ",\"txs\":" << blk.transactions.size();
                    out << "}";
                }

                out << "]}";
                res.set_header("Cache-Control", "no-store");
                res.set_content(out.str(), "application/json");
            } catch (const std::exception& e) {
                res.status = 500;
                res.set_content(std::string("{\"error\":\"") + e.what() + "\"}", "application/json");
            }
        });

        server_.Get("/explorer", [](const httplib::Request&, httplib::Response& res) {
            const char* html = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Caesar CZR - Block Explorer</title>
<style>
body { font-family: monospace; background: #0f1115; color: #e8e8e8; margin: 0; padding: 20px; }
h1 { color: #f0c040; }
.top { display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; flex-wrap: wrap; gap: 10px; }
button, a.btn { background: #f0c040; color: #000; border: none; padding: 10px 20px; font-weight: bold; border-radius: 6px; cursor: pointer; text-decoration: none; font-family: monospace; }
table { width: 100%; border-collapse: collapse; font-size: 13px; }
th { color: #f0c040; text-align: left; padding: 8px; border-bottom: 1px solid #333; }
td { padding: 8px; border-bottom: 1px solid #1a1a1a; }
tr:hover { background: #1a1a1a; }
.hash { font-size: 11px; color: #5fdc7a; word-break: break-all; }
#info { color: #5fdc7a; margin-bottom: 15px; }
</style>
</head>
<body>
<div class="top">
  <h1>Caesar CZR Explorer</h1>
  <div>
    <a class="btn" href="/">Wallet</a>
    <button onclick="load()">Refresh</button>
  </div>
</div>
<div id="info">Loading...</div>
<table>
  <thead>
    <tr><th>Height</th><th>Hash</th><th>Time</th><th>Diff</th><th>Nonce</th><th>Txs</th></tr>
  </thead>
  <tbody id="rows"></tbody>
</table>
<script>
async function load() {
  try {
    const r = await fetch('/api/blocks?limit=100', {cache:'no-store'});
    const d = await r.json();
    document.getElementById('info').textContent =
      'Height: ' + d.height + ' | Total: ' + d.count + ' blocks';
    const rows = document.getElementById('rows');
    rows.innerHTML = '';
    d.blocks.slice().reverse().forEach(b => {
      const tr = document.createElement('tr');
      const when = new Date(b.timestamp * 1000).toISOString().replace('T',' ').substring(0,19);
      tr.innerHTML =
        '<td>' + b.height + '</td>' +
        '<td class="hash">' + b.hash.substring(0,32) + '...</td>' +
        '<td>' + when + '</td>' +
        '<td>' + b.difficulty + '</td>' +
        '<td>' + b.nonce + '</td>' +
        '<td>' + b.txs + '</td>';
      rows.appendChild(tr);
    });
  } catch (e) {
    document.getElementById('info').textContent = 'Error: ' + e.message;
  }
}
load();
setInterval(load, 10000);
</script>
</body>
</html>)HTML";
            res.set_content(html, "text/html; charset=utf-8");
        });

        server_.Post("/api/wallet/sign", [this](const httplib::Request& req, httplib::Response& res) {
            if (!is_csrf_safe(req)) {
                res.status = 403;
                res.set_content("{\"error\":\"csrf\"}", "application/json");
                return;
            }
            try {
                if (!wallet_ || !wallet_->is_loaded()) {
                    throw std::runtime_error("wallet locked");
                }
                auto pos = req.body.find("\"message\"");
                if (pos == std::string::npos) throw std::runtime_error("missing message");
                auto start = req.body.find('"', pos + 10);
                auto end = req.body.find('"', start + 1);
                std::string message = req.body.substr(start + 1, end - start - 1);

                auto sig = caesar::sign_message(wallet_->private_key(), message);

                static const char* hexc = "0123456789abcdef";
                std::string sig_hex;
                sig_hex.reserve(sig.size() * 2);
                for (auto b : sig) {
                    sig_hex.push_back(hexc[(b >> 4) & 0xf]);
                    sig_hex.push_back(hexc[b & 0xf]);
                }

                res.set_content("{\"status\":\"ok\",\"signature\":\"" + sig_hex + "\"}", "application/json");
            } catch (const std::exception& e) {
                res.status = 400;
                res.set_content(std::string("{\"error\":\"") + e.what() + "\"}", "application/json");
            }
        });

        server_.Post("/api/wallet/verify", [this](const httplib::Request& req, httplib::Response& res) {
            try {
                if (!wallet_ || !wallet_->is_loaded()) {
                    throw std::runtime_error("wallet locked");
                }
                auto mpos = req.body.find("\"message\"");
                if (mpos == std::string::npos) throw std::runtime_error("missing message");
                auto mstart = req.body.find('"', mpos + 10);
                auto mend = req.body.find('"', mstart + 1);
                std::string message = req.body.substr(mstart + 1, mend - mstart - 1);

                auto spos = req.body.find("\"signature\"");
                if (spos == std::string::npos) throw std::runtime_error("missing signature");
                auto sstart = req.body.find('"', spos + 12);
                auto send = req.body.find('"', sstart + 1);
                std::string sig_hex = req.body.substr(sstart + 1, send - sstart - 1);

                auto sig_bytes = caesar::hex_to_bytes(sig_hex);

                EVP_PKEY* pub = wallet_->public_key_handle();
                if (!pub) throw std::runtime_error("no public key");

                const bool ok = caesar::verify_signature(pub, message, sig_bytes);

                res.set_content(std::string("{\"valid\":") + (ok ? "true" : "false") + "}", "application/json");
            } catch (const std::exception& e) {
                res.status = 400;
                res.set_content(std::string("{\"error\":\"") + e.what() + "\"}", "application/json");
            }
        });

        server_.Get("/api/wallet/qr.svg", [this](const httplib::Request&, httplib::Response& res) {
            if (!wallet_ || !wallet_->is_loaded()) { res.status = 400; return; }
            auto qr = qrcodegen::QrCode::encodeText(wallet_->address().c_str(), qrcodegen::QrCode::Ecc::MEDIUM);
            int n = qr.getSize(), b = 4, s = 6, t = (n + 2*b) * s;
            std::string o = "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 " + std::to_string(t) + " " + std::to_string(t) + "'><rect width='" + std::to_string(t) + "' height='" + std::to_string(t) + "' fill='#fff'/>";
            for (int y = 0; y < n; ++y) for (int x = 0; x < n; ++x) if (qr.getModule(x, y)) {
                o += "<rect x='" + std::to_string((x+b)*s) + "' y='" + std::to_string((y+b)*s) + "' width='" + std::to_string(s) + "' height='" + std::to_string(s) + "' fill='#000'/>";
            }
            o += "</svg>";
            res.set_content(o, "image/svg+xml");
        });

        server_.Get("/api/wallet/history", [this](const httplib::Request&, httplib::Response& res) {
            if (!wallet_ || !wallet_->is_loaded()) { res.status = 401; res.set_content("{\"error\":\"locked\"}", "application/json"); return; }
            std::string myaddr = wallet_->address();
            std::vector<Block> chain = node_.chain();
            std::ostringstream out;
            out << "{\"address\":\"" << myaddr << "\",\"transactions\":[";
            bool first = true; int count = 0;
            for (auto it = chain.rbegin(); it != chain.rend() && count < 100; ++it) {
                for (const auto& tx : it->transactions) {
                    for (const auto& o : tx.outputs) {
                        if (o.recipient == myaddr) {
                            if (!first) out << ",";
                            first = false;
                            out << "{\"block\":" << it->header.height
                                << ",\"timestamp\":" << it->header.timestamp
                                << ",\"amount\":" << o.amount
                                << ",\"direction\":\"received\"}";
                            ++count;
                            break;
                        }
                    }
                }
            }
            out << "]}";
            res.set_header("Cache-Control", "no-store");
            res.set_content(out.str(), "application/json");
        });

        server_.Post("/api/auth/change-pin", [this](const httplib::Request& req, httplib::Response& res) {
            if (!is_csrf_safe(req)) {
                res.status = 403;
                res.set_content("{\"error\":\"csrf\"}", "application/json");
                return;
            }
            try {
                auto op = req.body.find("\"old_pin\"");
                if (op == std::string::npos) throw std::runtime_error("missing old_pin");
                auto os = req.body.find('"', op + 10);
                auto oe = req.body.find('"', os + 1);
                std::string old_pin = req.body.substr(os + 1, oe - os - 1);

                auto np = req.body.find("\"new_pin\"");
                if (np == std::string::npos) throw std::runtime_error("missing new_pin");
                auto ns = req.body.find('"', np + 10);
                auto ne = req.body.find('"', ns + 1);
                std::string new_pin = req.body.substr(ns + 1, ne - ns - 1);

                if (old_pin.size() < 4) throw std::runtime_error("old PIN too short");
                if (new_pin.size() < 4) throw std::runtime_error("new PIN must be 4+ digits");

                if (!caesar::verify_pin(pin_path_, old_pin)) {
                    res.status = 401;
                    res.set_content("{\"error\":\"invalid old PIN\"}", "application/json");
                    return;
                }

                if (!wallet_->is_loaded()) {
                    wallet_->unlock_with_pin(old_pin);
                }

                wallet_->save_encrypted(new_pin);
                caesar::save_pin(pin_path_, new_pin);

                res.set_content("{\"status\":\"ok\"}", "application/json");
            } catch (const std::exception& e) {
                res.status = 400;
                res.set_content(std::string("{\"error\":\"") + e.what() + "\"}", "application/json");
            }
        });

        server_.Get("/manifest.json", [](const httplib::Request&, httplib::Response& res) {
            res.set_content(
                R"({"name":"Caesar CZR Wallet","short_name":"Caesar","start_url":"/","display":"standalone","background_color":"#0f1115","theme_color":"#f0c040","orientation":"portrait","icons":[{"src":"/icon-192.svg","sizes":"192x192","type":"image/svg+xml","purpose":"any maskable"}]})",
                "application/manifest+json");
        });

        server_.Get("/icon-192.svg", [](const httplib::Request&, httplib::Response& res) {
            res.set_content(
                R"(<svg xmlns="http://www.w3.org/2000/svg" width="192" height="192" viewBox="0 0 192 192"><rect width="192" height="192" rx="42" fill="#f0c040"/><text x="96" y="130" font-family="monospace" font-size="120" font-weight="bold" fill="#0f1115" text-anchor="middle">C</text></svg>)",
                "image/svg+xml");
        });

        server_.Get("/sw.js", [](const httplib::Request&, httplib::Response& res) {
            res.set_header("Cache-Control", "no-cache, no-store, must-revalidate");
            res.set_content(
                R"(self.addEventListener('install',e=>{self.skipWaiting();}); self.addEventListener('activate',e=>{e.waitUntil(caches.keys().then(ks=>Promise.all(ks.map(k=>caches.delete(k)))).then(()=>self.clients.claim()));}); self.addEventListener('fetch',e=>{if(e.request.url.includes('/api/'))return; if(e.request.mode==='navigate'){e.respondWith(fetch(e.request).catch(()=>caches.match(e.request)));return;} e.respondWith(fetch(e.request).catch(()=>caches.match(e.request)));});)",
                "application/javascript");
        });

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

        server_.Get("/api/wallet", [this](const httplib::Request& req, httplib::Response& res) {
            if (!is_authenticated(req)) {
                res.status = 401;
                res.set_content("{\"error\":\"unauthorized\"}", "application/json");
                return;
            }
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

        server_.Get("/api/balance", [this](const httplib::Request& req, httplib::Response& res) {
            if (!is_authenticated(req)) {
                res.status = 401;
                res.set_content("{\"error\":\"unauthorized\"}", "application/json");
                return;
            }
            std::ostringstream out;
            out << "{\"balance\":" << compute_balance() << "}";
            res.set_content(out.str(), "application/json");
        });

        server_.Get("/api/history", [this](const httplib::Request& req, httplib::Response& res) {
            if (!is_authenticated(req)) {
                res.status = 401;
                res.set_content("{\"error\":\"unauthorized\"}", "application/json");
                return;
            }
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

        server_.Post("/api/mine_default", [this](const httplib::Request& req,
                                                  httplib::Response& res) {
            if (!is_authenticated(req)) {
                res.status = 401;
                res.set_content("{\"error\":\"unauthorized\"}", "application/json");
                return;
            }
            if (!is_csrf_safe(req)) {
                res.status = 403;
                res.set_content("{\"error\":\"csrf\"}", "application/json");
                return;
            }
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

        // ============ POOL ENDPOINTS ============
        server_.Post("/api/pool/register", [this](const httplib::Request& req,
                                                  httplib::Response& res) {
            auto gf = [&](const std::string& key) -> std::string {
                auto pos = req.body.find("\"" + key + "\"");
                if (pos == std::string::npos) return "";
                auto colon = req.body.find(':', pos);
                if (colon == std::string::npos) return "";
                auto start = req.body.find('"', colon);
                if (start == std::string::npos) return "";
                auto end = req.body.find('"', start + 1);
                if (end == std::string::npos) return "";
                return req.body.substr(start + 1, end - start - 1);
            };
            std::string addr = gf("address");
            std::string name = gf("worker");
            if (addr.empty()) {
                res.status = 400;
                res.set_content("{\"ok\":false,\"error\":\"address required\"}",
                                "application/json");
                return;
            }
            std::lock_guard<std::mutex> lock(g_pool_mutex);
            if (!g_pool_state_loaded.exchange(true)) {
                pool_load_state_unlocked(wallet_path_.parent_path() / "pool_state.txt");
            }
            auto& w = g_pool_workers[addr];
            w.address = addr;
            if (!name.empty()) w.worker_name = name;
            w.last_seen = static_cast<std::uint64_t>(::time(nullptr));
            pool_save_state_unlocked(wallet_path_.parent_path() / "pool_state.txt");
            res.set_content("{\"ok\":true,\"address\":\"" + addr + "\"}",
                            "application/json");
        });

        server_.Get("/api/pool/job", [this](const httplib::Request& req,
                                            httplib::Response& res) {
            std::string addr;
            auto it = req.params.find("address");
            if (it != req.params.end()) addr = it->second;
            std::lock_guard<std::mutex> lock(g_pool_mutex);
            const std::size_t current_height = node_.height();
            bool need_new = !g_pool_current_job.active ||
                            g_pool_current_job.height != (current_height + 1);
            if (need_new) {
                std::string pool_recv = my_address();
                if (pool_recv.empty()) pool_recv = "CAESAR_POOL_COINBASE";
                std::string recipient = pool_recv;
                try {
                    Block candidate = node_.build_pool_candidate(recipient);
                    g_pool_job_counter++;
                    g_pool_current_job.job_id = g_pool_job_counter;
                    g_pool_current_job.candidate = candidate;
                    g_pool_current_job.height = candidate.header.height;
                    g_pool_current_job.block_difficulty = candidate.header.difficulty;
                    std::uint32_t sd = candidate.header.difficulty > 8
                                       ? candidate.header.difficulty - 8 : 1;
                    g_pool_current_job.share_difficulty = sd;
                    g_pool_current_job.header_hex = pool_bytes_to_hex(candidate.pow_header());
                    g_pool_current_job.miner_address = recipient;
                    g_pool_current_job.active = true;
                } catch (const std::exception& e) {
                    res.status = 500;
                    res.set_content(std::string("{\"error\":\"") + e.what() + "\"}",
                                    "application/json");
                    return;
                }
            }
            std::ostringstream out;
            out << "{\"ok\":true"
                << ",\"job_id\":" << g_pool_current_job.job_id
                << ",\"header_hex\":\"" << g_pool_current_job.header_hex << "\""
                << ",\"share_difficulty\":" << g_pool_current_job.share_difficulty
                << ",\"block_difficulty\":" << g_pool_current_job.block_difficulty
                << ",\"height\":" << g_pool_current_job.height
                << "}";
            res.set_content(out.str(), "application/json");
        });

        server_.Post("/api/pool/submit", [this](const httplib::Request& req,
                                                httplib::Response& res) {
            auto gs = [&](const std::string& key) -> std::string {
                auto pos = req.body.find("\"" + key + "\"");
                if (pos == std::string::npos) return "";
                auto colon = req.body.find(':', pos);
                if (colon == std::string::npos) return "";
                auto start = req.body.find('"', colon);
                if (start == std::string::npos) return "";
                auto end = req.body.find('"', start + 1);
                if (end == std::string::npos) return "";
                return req.body.substr(start + 1, end - start - 1);
            };
            auto gu = [&](const std::string& key) -> std::uint64_t {
                auto pos = req.body.find("\"" + key + "\"");
                if (pos == std::string::npos) return 0;
                auto colon = req.body.find(':', pos);
                if (colon == std::string::npos) return 0;
                auto start = req.body.find_first_of("0123456789", colon);
                if (start == std::string::npos) return 0;
                auto end = req.body.find_first_not_of("0123456789", start);
                if (end == std::string::npos) end = req.body.size();
                try { return std::stoull(req.body.substr(start, end - start)); }
                catch (...) { return 0; }
            };
            std::string addr = gs("address");
            std::uint64_t job_id = gu("job_id");
            std::uint64_t nonce = gu("nonce");
            if (addr.empty()) {
                res.status = 400;
                res.set_content("{\"ok\":false,\"error\":\"address required\"}",
                                "application/json");
                return;
            }
            std::lock_guard<std::mutex> lock(g_pool_mutex);
            if (!g_pool_current_job.active || g_pool_current_job.job_id != job_id) {
                res.set_content("{\"ok\":false,\"stale\":true}", "application/json");
                return;
            }
            auto header_bytes = pool_hex_to_bytes(g_pool_current_job.header_hex);
            Hash256 h = calculate_pow_hash(header_bytes, nonce);
            bool credited = false;
            if (pow_meets_difficulty(h, g_pool_current_job.share_difficulty)) {
                g_pool_workers[addr].shares++;
                g_pool_workers[addr].shares_pending++;
                g_pool_workers[addr].last_seen =
                    static_cast<std::uint64_t>(::time(nullptr));
                g_pool_total_shares++;
                credited = true;
            }
            bool is_block = pow_meets_difficulty(h, g_pool_current_job.block_difficulty);
            bool block_added = false;
            if (is_block) {
                Block solved = g_pool_current_job.candidate;
                solved.header.nonce = nonce;
                try {
                    if (node_.submit_pool_solution(solved)) {
                        block_added = true;
                        g_pool_total_blocks++;
                        g_pool_current_job.active = false;
                    }
                } catch (...) {}
            }
            if (credited || block_added) {
                pool_save_state_unlocked(wallet_path_.parent_path() / "pool_state.txt");
            }
            std::ostringstream out;
            out << "{\"ok\":true"
                << ",\"credited\":" << (credited ? "true" : "false")
                << ",\"is_block\":" << (is_block ? "true" : "false")
                << ",\"block_added\":" << (block_added ? "true" : "false")
                << "}";
            res.set_content(out.str(), "application/json");
        });

        server_.Get("/api/pool/stats", [this](const httplib::Request&,
                                              httplib::Response& res) {
            {
                std::lock_guard<std::mutex> lock(g_pool_mutex);
                if (!g_pool_state_loaded.exchange(true)) {
                    pool_load_state_unlocked(wallet_path_.parent_path() / "pool_state.txt");
                }
                std::ostringstream out;
                out << "{\"ok\":true"
                    << ",\"fee_percent\":" << g_pool_fee_percent
                    << ",\"total_shares\":" << g_pool_total_shares
                    << ",\"total_blocks\":" << g_pool_total_blocks
                    << ",\"current_height\":" << node_.height()
                    << ",\"auto_enabled\":" << (g_pool_auto_enabled.load() ? "true" : "false")
                    << ",\"auto_blocks\":" << g_pool_auto_blocks.load()
                    << ",\"last_payout_height\":" << g_pool_last_payout_height.load()
                    << ",\"workers\":[";
                bool first = true;
                for (const auto& kv : g_pool_workers) {
                    if (!first) out << ",";
                    first = false;
                    out << "{\"address\":\"" << kv.second.address << "\""
                        << ",\"name\":\"" << kv.second.worker_name << "\""
                        << ",\"shares\":" << kv.second.shares << "}";
                }
                out << "]}";
                res.set_content(out.str(), "application/json");
            }
            pool_maybe_auto_payout();
        });

        server_.Get("/api/pool/payouts", [](const httplib::Request&,
                                            httplib::Response& res) {
            std::lock_guard<std::mutex> lock(g_pool_mutex);
            std::ostringstream out;
            out << "{\"ok\":true,\"count\":" << g_pool_payouts.size() << ",\"items\":[";
            bool first = true;
            for (const auto& r : g_pool_payouts) {
                if (!first) out << ",";
                first = false;
                out << "{\"ts\":" << r.timestamp
                    << ",\"block\":" << r.block_height
                    << ",\"txid\":\"" << r.txid << "\""
                    << ",\"total\":" << r.total_amount
                    << ",\"fee\":" << r.fee_amount
                    << ",\"recipients\":" << r.recipient_count << "}";
            }
            out << "]}";
            res.set_content(out.str(), "application/json");
        });

        server_.Get("/pool", [](const httplib::Request&, httplib::Response& res) {
            const char* html = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Caesar CZR Pool</title>
<style>
body{font-family:monospace;background:#0f1115;color:#e8e8e8;margin:0;padding:16px}
h1{color:#f0c040;font-size:22px;margin:0 0 12px}
h2{color:#f0c040;font-size:15px;margin:14px 0 6px}
.card{background:#1a1d24;padding:12px;margin:8px 0;border-radius:8px;border:1px solid #252a33}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:8px}
.stat{background:#0a0d12;padding:10px;border-radius:6px;border:1px solid #1f242c}
.stat .label{color:#888;font-size:11px;text-transform:uppercase}
.stat .value{color:#5fdc7a;font-weight:bold;font-size:18px;margin-top:4px;word-break:break-all}
table{width:100%;border-collapse:collapse;font-size:12px}
th{color:#f0c040;text-align:left;padding:6px;border-bottom:1px solid #333}
td{padding:6px;border-bottom:1px solid #1a1a1a;word-break:break-all}
tr:hover{background:#1a1a1a}
.hash{color:#5fdc7a;font-size:11px}
button,a.btn{background:#f0c040;color:#000;border:none;padding:10px 16px;font-weight:bold;border-radius:6px;cursor:pointer;font-family:monospace;font-size:13px;margin:2px 3px 2px 0;text-decoration:none;display:inline-block}
button:active{background:#d0a020}
#log{background:#000;padding:8px;border-radius:4px;font-size:11px;max-height:120px;overflow-y:auto;color:#5fdc7a;margin-top:8px}
.top{display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:8px;margin-bottom:12px}
</style>
</head>
<body>
<div class="top">
  <h1>Caesar CZR Pool</h1>
  <div>
    <button onclick="doPayout()">Payout Now</button>
    <button onclick="refresh()">Refresh</button>
    <a class="btn" href="/">Wallet</a>
    <a class="btn" href="/explorer">Explorer</a>
  </div>
</div>

<div class="card">
  <h2>Chain</h2>
  <div class="grid" id="chainGrid">
    <div class="stat"><div class="label">Height</div><div class="value" id="vHeight">-</div></div>
    <div class="stat"><div class="label">Mempool</div><div class="value" id="vMempool">-</div></div>
    <div class="stat"><div class="label">Peers</div><div class="value" id="vPeers">-</div></div>
    <div class="stat"><div class="label">Uptime</div><div class="value" id="vUptime">-</div></div>
  </div>
</div>

<div class="card">
  <h2>Pool</h2>
  <div class="grid">
    <div class="stat"><div class="label">Total Shares</div><div class="value" id="vShares">-</div></div>
    <div class="stat"><div class="label">Total Blocks</div><div class="value" id="vBlocks">-</div></div>
    <div class="stat"><div class="label">Fee %</div><div class="value" id="vFee">-</div></div>
    <div class="stat"><div class="label">Workers</div><div class="value" id="vWorkers">-</div></div>
  </div>
</div>

<div class="card">
  <h2>Workers</h2>
  <table id="workersTable">
    <thead><tr><th>Address</th><th>Name</th><th>Shares</th></tr></thead>
    <tbody></tbody>
  </table>
</div>

<div class="card">
  <h2>Payouts</h2>
  <table id="payoutsTable">
    <thead><tr><th>Time</th><th>Block</th><th>TXID</th><th>Total</th><th>Fee</th></tr></thead>
    <tbody></tbody>
  </table>
</div>

<div class="card">
  <h2>Log</h2>
  <div id="log">Ready.</div>
</div>

<script>
function log(s){var el=document.getElementById('log');el.innerHTML=el.innerHTML+'<br>'+new Date().toLocaleTimeString()+' | '+s;el.scrollTop=el.scrollHeight;}

async function refresh(){
  try{
    var s=await fetch('/api/status').then(r=>r.json());
    document.getElementById('vHeight').textContent=s.height;
    document.getElementById('vMempool').textContent=s.mempool;
    document.getElementById('vPeers').textContent=s.peers;
    document.getElementById('vUptime').textContent=s.uptime+'s';

    var p=await fetch('/api/pool/stats').then(r=>r.json());
    document.getElementById('vShares').textContent=p.total_shares;
    document.getElementById('vBlocks').textContent=p.total_blocks;
    document.getElementById('vFee').textContent=p.fee_percent+'%';
    document.getElementById('vWorkers').textContent=p.workers.length;

    var wt=document.querySelector('#workersTable tbody');
    wt.innerHTML='';
    p.workers.forEach(function(w){
      var tr=document.createElement('tr');
      tr.innerHTML='<td>'+w.address.slice(0,20)+'...</td><td>'+w.name+'</td><td>'+w.shares+'</td>';
      wt.appendChild(tr);
    });

    var pr=await fetch('/api/pool/payouts').then(r=>r.json());
    var pt=document.querySelector('#payoutsTable tbody');
    pt.innerHTML='';
    pr.items.slice().reverse().forEach(function(x){
      var tr=document.createElement('tr');
      tr.innerHTML='<td>'+(new Date(x.ts*1000)).toLocaleTimeString()+'</td>'+
                   '<td>'+x.block+'</td>'+
                   '<td class="hash">'+x.txid.slice(0,24)+'...</td>'+
                   '<td>'+(x.total/100000000).toFixed(2)+'</td>'+
                   '<td>'+(x.fee/100000000).toFixed(4)+'</td>';
      pt.appendChild(tr);
    });
  }catch(e){log('refresh error: '+e.message);}
}

async function doPayout(){
  log('triggering payout...');
  try{
    var r=await fetch('/api/pool/payout',{method:'POST'});
    var d=await r.json();
    log(JSON.stringify(d));
    if(d.ok) log('payout txid: '+d.txid);
    refresh();
  }catch(e){log('payout error: '+e.message);}
}

refresh();
setInterval(refresh,5000);
</script>
</body>
</html>)HTML";
            res.set_content(html, "text/html");
        });

        server_.Get("/api/pool/auto/status", [](const httplib::Request&,
                                                    httplib::Response& res) {
            std::ostringstream out;
            out << "{\"ok\":true"
                << ",\"enabled\":" << (g_pool_auto_enabled.load() ? "true" : "false")
                << ",\"blocks\":" << g_pool_auto_blocks.load()
                << ",\"last_payout_height\":" << g_pool_last_payout_height.load()
                << "}";
            res.set_content(out.str(), "application/json");
        });

        server_.Post("/api/pool/auto/config", [this](const httplib::Request& req,
                                                     httplib::Response& res) {
            if (!is_authenticated(req)) {
                res.status = 401;
                res.set_content("{\"error\":\"unauthorized\"}", "application/json");
                return;
            }
            auto get_u64 = [&](const std::string& key) -> std::uint64_t {
                auto pos = req.body.find("\"" + key + "\"");
                if (pos == std::string::npos) return 0;
                auto colon = req.body.find(':', pos);
                if (colon == std::string::npos) return 0;
                auto s = req.body.find_first_of("0123456789", colon);
                if (s == std::string::npos) return 0;
                auto e = req.body.find_first_not_of("0123456789", s);
                if (e == std::string::npos) e = req.body.size();
                try { return std::stoull(req.body.substr(s, e - s)); } catch (...) { return 0; }
            };
            if (req.body.find("\"enabled\"") != std::string::npos) {
                bool on = (req.body.find("\"enabled\":true") != std::string::npos ||
                           req.body.find("\"enabled\": true") != std::string::npos);
                g_pool_auto_enabled.store(on);
            }
            std::uint64_t blk = get_u64("blocks");
            if (blk >= 1 && blk <= 10000)
                g_pool_auto_blocks.store(static_cast<std::uint32_t>(blk));
            {
                std::lock_guard<std::mutex> lock(g_pool_mutex);
                pool_save_state_unlocked(wallet_path_.parent_path() / "pool_state.txt");
            }
            std::ostringstream out;
            out << "{\"ok\":true"
                << ",\"enabled\":" << (g_pool_auto_enabled.load() ? "true" : "false")
                << ",\"blocks\":" << g_pool_auto_blocks.load() << "}";
            res.set_content(out.str(), "application/json");
        });

        // ============ END POOL ENDPOINTS ============

        server_.Post("/api/send", [this](const httplib::Request& req, httplib::Response& res) {
            if (!is_authenticated(req)) {
                res.status = 401;
                res.set_content("{\"error\":\"unauthorized\"}", "application/json");
                return;
            }
            if (!is_csrf_safe(req)) {
                res.status = 403;
                res.set_content("{\"error\":\"csrf\"}", "application/json");
                return;
            }
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
