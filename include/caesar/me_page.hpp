// Copyright (c) Caesar contributors. See LICENSE.
//
// Wallet page for invited users, served at /me.
// It only talks to the /api/me/* routes, which work on the logged-in user's OWN wallet.
#pragma once

namespace caesar {

inline const char* const kCzrMePage = R"ME(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0a0e17">
<title>Caesar Wallet</title>
<style>
:root{--bg:#0a0e17;--card:#131a2b;--line:#222c47;--gold:#f0c040;--green:#4dff92;--red:#ff5c7a;--mut:#8fa0c8;--txt:#e8ecf5}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--txt);font-family:system-ui,-apple-system,Roboto,sans-serif;padding-bottom:84px}
.wrap{max-width:460px;margin:0 auto;padding:16px}
header{display:flex;align-items:center;gap:10px;padding:14px 16px;border-bottom:1px solid var(--line);position:sticky;top:0;background:var(--bg);z-index:5}
header .logo{width:34px;height:34px;border-radius:10px;background:linear-gradient(135deg,#f0c040,#c9a02a);display:flex;align-items:center;justify-content:center;font-size:18px}
header b{font-size:18px}
header .sp{flex:1}
.pill{font-size:12px;color:var(--mut);border:1px solid var(--line);border-radius:999px;padding:4px 10px}
.card{background:var(--card);border:1px solid var(--line);border-radius:18px;padding:16px;margin:14px 0}
h2{margin:0 0 10px;font-size:13px;letter-spacing:1.5px;color:var(--gold);text-transform:uppercase}
.sm{font-size:12px;color:var(--mut)}
.mono{font-family:ui-monospace,Menlo,Consolas,monospace;word-break:break-all}
.bal{font-size:34px;font-weight:700;color:var(--gold);margin:4px 0}
.bal small{font-size:15px;color:var(--mut);margin-left:6px}
input,textarea,button,select{width:100%;padding:13px;margin:6px 0;border-radius:12px;border:1px solid #33406a;background:#0b1120;color:#fff;font-size:16px;font-family:inherit}
textarea{min-height:70px;resize:vertical}
button{background:linear-gradient(135deg,#f0c040,#c9a02a);color:#111;font-weight:700;border:0;cursor:pointer}
button.alt{background:#1a2440;color:#fff;border:1px solid #33406a}
button.sm2{width:auto;padding:8px 14px;margin:0;font-size:13px}
.row{display:flex;gap:8px;align-items:center}
.row>*{flex:1}
.row>button{flex:0 0 auto;width:auto}
.msg{min-height:18px;font-size:13px;margin-top:4px}
.err{color:var(--red)}.ok{color:var(--green)}
.tx{padding:10px 0;border-bottom:1px solid var(--line);font-size:13px}
.tx:last-child{border-bottom:0}
.tx .a{font-weight:700}
.in{color:var(--green)}.out{color:var(--red)}
nav{position:fixed;left:0;right:0;bottom:0;background:#0d1322;border-top:1px solid var(--line);display:flex;padding-bottom:env(safe-area-inset-bottom)}
nav button{flex:1;margin:0;border-radius:0;background:transparent;color:var(--mut);border:0;padding:14px 4px;font-size:12px;font-weight:600}
nav button.on{color:var(--gold)}
.tab{display:none}.tab.on{display:block}
.qr{background:#fff;border-radius:14px;padding:8px;width:200px;height:200px;margin:10px auto;display:block}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.stat{background:#0b1120;border:1px solid var(--line);border-radius:12px;padding:10px}
.stat b{display:block;font-size:20px;color:var(--green)}
.lvl{display:grid;grid-template-columns:repeat(4,1fr);gap:8px;margin-top:8px}
.lvl div{background:#0b1120;border:1px solid var(--line);border-radius:10px;padding:8px;text-align:center;font-size:12px;color:var(--mut)}
.lvl b{display:block;font-size:18px;color:var(--txt)}
.warn{background:#2a1d10;border:1px solid #6b4a1a;border-radius:12px;padding:10px;font-size:12px;color:#ffd48a;margin-top:10px}
.phrase{background:#0b1120;border:1px dashed var(--gold);border-radius:12px;padding:12px;font-size:15px;line-height:1.7;margin-top:8px}
.ab{display:flex;align-items:center;gap:8px;padding:8px 0;border-bottom:1px solid var(--line)}
.ab .n{font-weight:700;min-width:70px}
.ab .ad{flex:1;font-size:11px;color:var(--mut)}
</style></head><body>

<div id="loginView" class="wrap" style="display:none">
  <div class="card" style="margin-top:40px">
    <h2>Caesar Wallet</h2>
    <div class="sm">Sign in with the username and PIN you created when you joined.</div>
    <input id="u" placeholder="Username" autocapitalize="none" autocomplete="username">
    <input id="p" type="password" inputmode="numeric" placeholder="PIN" autocomplete="current-password">
    <button id="go">OPEN WALLET</button>
    <div id="lmsg" class="msg err"></div>
  </div>
</div>

<div id="app" style="display:none">
<header><div class="logo">&#9889;</div><b>Caesar</b><div class="sp"></div><div class="pill" id="hdr">&nbsp;</div></header>
<div class="wrap">

<section id="t_wallet" class="tab on">
  <div class="card">
    <div class="sm">Balance</div>
    <div class="bal"><span id="bal">0</span><small>CZR</small></div>
    <div id="pend" class="sm"></div>
  </div>
  <div class="card">
    <h2>Receive</h2>
    <img id="qr" class="qr" alt="QR code of your address">
    <div class="sm" style="text-align:center">Your address</div>
    <div id="addr" class="mono" style="font-size:12px;text-align:center;margin:6px 0"></div>
    <button class="alt" id="copyAddr">Copy address</button>
  </div>
  <div class="card">
    <h2>Activity</h2>
    <div id="txs"><div class="sm">Loading&hellip;</div></div>
  </div>
  <div class="card">
    <h2>Your circle</h2>
    <div class="sm" id="circleSub"></div>
    <div class="lvl" id="lvls"></div>
    <div class="sm" style="margin-top:12px">Your invite link</div>
    <div class="row"><input id="inv" readonly><button class="sm2" id="copyInv">Copy</button></div>
    <button class="alt" id="shareInv">Share invite</button>
  </div>
</section>

<section id="t_send" class="tab">
  <div class="card">
    <h2>Send CZR</h2>
    <div class="sm">Available: <b id="availBal">0</b> CZR</div>
    <input id="sTo" placeholder="CZ1... recipient address" autocapitalize="none" autocomplete="off">
    <select id="sPick"><option value="">Pick from address book&hellip;</option></select>
    <input id="sAmt" inputmode="decimal" placeholder="Amount in CZR (e.g. 1.5)">
    <input id="sPin" type="password" inputmode="numeric" placeholder="Your PIN" autocomplete="off">
    <button id="sendBtn">Send</button>
    <div id="sMsg" class="msg"></div>
  </div>
</section>

<section id="t_explore" class="tab">
  <div class="card">
    <h2>Network</h2>
    <div class="grid">
      <div class="stat"><span class="sm">Height</span><b id="nH">-</b></div>
      <div class="stat"><span class="sm">Peers</span><b id="nP">-</b></div>
      <div class="stat"><span class="sm">Mempool</span><b id="nM">-</b></div>
      <div class="stat"><span class="sm">Your rank</span><b id="nU">-</b></div>
    </div>
  </div>
  <div class="card">
    <h2>Latest blocks</h2>
    <div id="blocks"></div>
    <button class="alt" id="refNet">Refresh</button>
  </div>
</section>

<section id="t_more" class="tab">
  <div class="card">
    <h2>Sign / Verify</h2>
    <div class="sm">Sign a message with your private key. Anyone can verify it.</div>
    <textarea id="mMsg" placeholder="message to sign"></textarea>
    <input id="mPin" type="password" inputmode="numeric" placeholder="Your PIN" autocomplete="off">
    <button id="signBtn">Sign message</button>
    <textarea id="mSig" placeholder="signature (hex)"></textarea>
    <div class="sm">Verify a signature</div>
    <textarea id="vMsg" placeholder="message to verify"></textarea>
    <input id="vSig" placeholder="signature (hex)" autocomplete="off">
    <input id="vKey" placeholder="signer public key (hex)" autocomplete="off">
    <button class="alt" id="verBtn">Verify signature</button>
    <div id="mMsg2" class="msg"></div>
  </div>
  <div class="card">
    <h2>Address book</h2>
    <div class="sm">Save addresses for one-tap sending. Stored only on this device.</div>
    <input id="abName" placeholder="name (e.g. friend)">
    <div class="row"><input id="abAddr" placeholder="CZ1... address" autocapitalize="none"><button class="sm2" id="abAdd">+ Add</button></div>
    <div id="abList"></div>
    <div id="abMsg" class="msg"></div>
  </div>
  <div class="card">
    <h2>Change PIN</h2>
    <input id="cOld" type="password" inputmode="numeric" placeholder="Current PIN" autocomplete="off">
    <input id="cNew" type="password" inputmode="numeric" placeholder="New PIN (8+ digits)" autocomplete="off">
    <button class="alt" id="pinBtn">Change PIN</button>
    <div id="cMsg" class="msg"></div>
  </div>
  <div class="card">
    <h2>Recovery phrase</h2>
    <div class="sm">Your 12 words are the only way to restore this wallet. Write them on paper.</div>
    <input id="bPin" type="password" inputmode="numeric" placeholder="Your PIN" autocomplete="off">
    <button class="alt" id="bkBtn">Show recovery phrase</button>
    <div id="phrase"></div>
    <div id="bMsg" class="msg"></div>
    <div class="warn">Never share these words. Anyone who has them controls your coins. Your wallet key is stored encrypted on the node that hosts this page, protected by your PIN.</div>
  </div>
  <div class="card">
    <button class="alt" id="out">Log out</button>
  </div>
</section>

</div>
<nav>
  <button data-t="wallet" class="on">Wallet</button>
  <button data-t="send">Send</button>
  <button data-t="explore">Explore</button>
  <button data-t="more">More</button>
</nav>
</div>

<script>
function $(i){return document.getElementById(i);}
function fixDigits(s){return String(s||'').replace(/[٠-٩]/g,function(c){return String(c.charCodeAt(0)-1632);}).replace(/[۰-۹]/g,function(c){return String(c.charCodeAt(0)-1776);});}
async function api(path,opt){
  const o=Object.assign({cache:'no-store',credentials:'same-origin'},opt||{});
  const r=await fetch(path,o);let d={};try{d=await r.json();}catch(e){}
  return {s:r.status,d:d};
}
function post(path,obj){return api(path,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(obj)});}
function fmt(atoms){
  const n=Number(atoms)||0;const w=Math.floor(n/1e8);const f=String(n%1e8).padStart(8,'0').replace(/0+$/,'');
  return w.toLocaleString('en-US')+(f?'.'+f:'');
}
function toAtoms(s){
  s=fixDigits(s).trim().replace(',','.');
  if(!/^\d+(\.\d{1,8})?$/.test(s))return null;
  const p=s.split('.');const frac=((p[1]||'')+'00000000').slice(0,8);
  const v=Number(p[0])*1e8+Number(frac);
  if(!isFinite(v)||v<=0||v>9e15)return null;
  return String(v);
}
function setMsg(el,t,good){el.textContent=t||'';el.className='msg '+(good?'ok':'err');}
function short(s,n){s=String(s||'');return s.length>n?s.slice(0,n)+'…':s;}
function copy(text,btn){
  const done=function(){if(btn){const o=btn.textContent;btn.textContent='Copied';setTimeout(function(){btn.textContent=o;},1200);}};
  if(navigator.clipboard&&navigator.clipboard.writeText){navigator.clipboard.writeText(text).then(done,function(){});}
  else{const t=document.createElement('textarea');t.value=text;document.body.appendChild(t);t.select();try{document.execCommand('copy');done();}catch(e){}document.body.removeChild(t);}
}

let ME=null,BAL=0;

function showApp(on){$('loginView').style.display=on?'none':'block';$('app').style.display=on?'block':'none';}

document.querySelectorAll('nav button').forEach(function(b){
  b.onclick=function(){
    document.querySelectorAll('nav button').forEach(function(x){x.classList.toggle('on',x===b);});
    document.querySelectorAll('.tab').forEach(function(t){t.classList.toggle('on',t.id==='t_'+b.getAttribute('data-t'));});
    if(b.getAttribute('data-t')==='explore')loadNet();
    window.scrollTo(0,0);
  };
});

async function loadBalance(){
  const b=await api('/api/me/balance');
  if(b.s!==200){return false;}
  BAL=Number(b.d.balance)||0;
  $('bal').textContent=fmt(BAL);$('availBal').textContent=fmt(BAL);
  return true;
}
async function loadTxs(){
  const h=await api('/api/me/txs');
  const box=$('txs');box.textContent='';
  const items=(h.d&&h.d.transactions)||[];
  let pending=0;
  items.forEach(function(t){
    if(t.pending)pending++;
    const e=document.createElement('div');e.className='tx';
    const a=document.createElement('div');a.className='a '+(t.type==='sent'?'out':'in');
    a.textContent=(t.type==='sent'?'- ':'+ ')+fmt(t.amount)+' CZR'+(t.pending?'  (pending)':'');
    const m=document.createElement('div');m.className='sm mono';
    m.textContent=(t.type==='sent'?'to '+short(t.peer,22):'received')+' · block '+t.block+' · '+short(t.txid,12);
    e.appendChild(a);e.appendChild(m);box.appendChild(e);
  });
  if(!items.length){const d=document.createElement('div');d.className='sm';d.textContent='No transactions yet';box.appendChild(d);}
  $('pend').textContent=pending?pending+' pending transaction(s)':'';
}
async function loadInfo(){
  const r=await api('/api/me/info');
  if(r.s!==200)return false;
  ME=r.d;
  $('hdr').textContent=ME.username+' · #'+ME.user_number;
  $('addr').textContent=ME.address;
  $('qr').src='/api/wallet/qr.svg?address='+encodeURIComponent(ME.address);
  $('inv').value=location.origin+'/?ref='+ME.address;
  $('nU').textContent='#'+ME.user_number;
  const lv=ME.levels||[0,0,0,0];let tot=0;
  const box=$('lvls');box.textContent='';
  lv.forEach(function(n,i){tot+=n;const d=document.createElement('div');d.textContent='Level '+(i+1);const b=document.createElement('b');b.textContent=n;d.appendChild(b);box.appendChild(d);});
  $('circleSub').textContent=(ME.is_founder?'Founder #'+ME.user_number+'. ':'')+(tot?'Your circle has '+tot+' people.':'Invite friends with your link below.');
  return true;
}
async function loadNet(){
  const r=await api('/api/me/network');
  if(r.s!==200)return;
  $('nH').textContent=r.d.height;$('nP').textContent=r.d.peers;$('nM').textContent=r.d.mempool;
  const box=$('blocks');box.textContent='';
  (r.d.blocks||[]).forEach(function(b){
    const e=document.createElement('div');e.className='tx';
    const a=document.createElement('div');a.className='a';a.textContent='#'+b.height+'  ·  '+b.txs+' tx';
    const m=document.createElement('div');m.className='sm mono';
    m.textContent=new Date(b.time*1000).toLocaleTimeString()+' · '+short(b.hash,20);
    e.appendChild(a);e.appendChild(m);box.appendChild(e);
  });
}
async function refresh(){await Promise.all([loadBalance(),loadTxs()]);}

async function boot(){
  const ok=await loadInfo();
  if(!ok){showApp(false);return;}
  showApp(true);
  renderAB();
  refresh();
}

$('go').onclick=async function(){
  const u=$('u').value.trim(),p=fixDigits($('p').value);
  $('lmsg').textContent='';
  if(!u||p.length<4){$('lmsg').textContent='Enter your username and PIN';return;}
  const r=await post('/api/me/login',{username:u,pin:p});
  $('p').value='';
  if(r.d&&r.d.ok){await boot();}else{$('lmsg').textContent=(r.d&&r.d.error)||'Login failed';}
};
$('p').addEventListener('keydown',function(e){if(e.key==='Enter')$('go').click();});
$('out').onclick=async function(){await api('/api/me/logout',{method:'POST'});ME=null;showApp(false);};
$('copyAddr').onclick=function(){if(ME)copy(ME.address,$('copyAddr'));};
$('copyInv').onclick=function(){copy($('inv').value,$('copyInv'));};
$('shareInv').onclick=function(){
  const url=$('inv').value;
  if(navigator.share){navigator.share({title:'Join Caesar',text:'Join me on Caesar CZR:',url:url}).catch(function(){});}
  else{copy(url,$('shareInv'));}
};
$('refNet').onclick=loadNet;

$('sendBtn').onclick=async function(){
  const to=$('sTo').value.trim(),amt=toAtoms($('sAmt').value),pin=fixDigits($('sPin').value);
  const m=$('sMsg');setMsg(m,'');
  if(!/^CZ1[0-9a-f]{20,}$/i.test(to)){setMsg(m,'Enter a valid CZ1... address');return;}
  if(!amt){setMsg(m,'Enter a valid amount');return;}
  if(!pin){setMsg(m,'Enter your PIN');return;}
  if(Number(amt)>BAL){setMsg(m,'Insufficient balance');return;}
  if(!confirm('Send '+fmt(amt)+' CZR to\n'+to+' ?'))return;
  $('sendBtn').disabled=true;
  const r=await post('/api/me/send',{recipient:to,amount:amt,pin:pin});
  $('sendBtn').disabled=false;$('sPin').value='';
  if(r.d&&r.d.ok){setMsg(m,'Sent. Transaction '+short(r.d.txid,16),true);$('sAmt').value='';await refresh();}
  else{setMsg(m,(r.d&&r.d.error)||'Send failed');}
};
$('sPick').onchange=function(){if($('sPick').value)$('sTo').value=$('sPick').value;};

$('signBtn').onclick=async function(){
  const m=$('mMsg2');setMsg(m,'');
  const msg=$('mMsg').value,pin=fixDigits($('mPin').value);
  if(!msg){setMsg(m,'Write a message first');return;}
  if(!pin){setMsg(m,'Enter your PIN');return;}
  const r=await post('/api/me/sign',{message:msg,pin:pin});
  $('mPin').value='';
  if(r.d&&r.d.ok){$('mSig').value=r.d.signature;$('vMsg').value=msg;$('vSig').value=r.d.signature;$('vKey').value=r.d.public_key;setMsg(m,'Signed',true);}
  else{setMsg(m,(r.d&&r.d.error)||'Signing failed');}
};
$('verBtn').onclick=async function(){
  const m=$('mMsg2');setMsg(m,'');
  const r=await post('/api/me/verify',{message:$('vMsg').value,signature:$('vSig').value.trim(),public_key:$('vKey').value.trim()});
  if(r.d&&r.d.ok){setMsg(m,r.d.valid?'Valid signature by '+short(r.d.signer,24):'INVALID signature',!!r.d.valid);}
  else{setMsg(m,(r.d&&r.d.error)||'Verify failed');}
};

function abKey(){return 'czr_ab_'+(ME?ME.address:'');}
function abGet(){try{return JSON.parse(localStorage.getItem(abKey())||'[]');}catch(e){return [];}}
function abSet(v){try{localStorage.setItem(abKey(),JSON.stringify(v));}catch(e){}}
function renderAB(){
  const list=abGet(),box=$('abList'),pick=$('sPick');
  box.textContent='';
  while(pick.options.length>1)pick.remove(1);
  list.forEach(function(it,i){
    const r=document.createElement('div');r.className='ab';
    const n=document.createElement('div');n.className='n';n.textContent=it.name;
    const a=document.createElement('div');a.className='ad mono';a.textContent=short(it.addr,26);
    const s=document.createElement('button');s.className='sm2';s.textContent='Send';
    s.onclick=function(){$('sTo').value=it.addr;document.querySelector('nav button[data-t=send]').click();};
    const d=document.createElement('button');d.className='sm2 alt';d.textContent='✕';
    d.onclick=function(){const l=abGet();l.splice(i,1);abSet(l);renderAB();};
    r.appendChild(n);r.appendChild(a);r.appendChild(s);r.appendChild(d);box.appendChild(r);
    const o=document.createElement('option');o.value=it.addr;o.textContent=it.name;pick.appendChild(o);
  });
  if(!list.length){const e=document.createElement('div');e.className='sm';e.style.padding='10px 0';e.textContent='No saved addresses yet';box.appendChild(e);}
}
$('abAdd').onclick=function(){
  const name=$('abName').value.trim().slice(0,24),addr=$('abAddr').value.trim();
  const m=$('abMsg');setMsg(m,'');
  if(!name){setMsg(m,'Enter a name');return;}
  if(!/^CZ1[0-9a-f]{20,}$/i.test(addr)){setMsg(m,'Enter a valid CZ1... address');return;}
  const l=abGet();l.push({name:name,addr:addr});abSet(l);
  $('abName').value='';$('abAddr').value='';renderAB();
};

$('pinBtn').onclick=async function(){
  const m=$('cMsg');setMsg(m,'');
  const o=fixDigits($('cOld').value),n=fixDigits($('cNew').value);
  if(!o){setMsg(m,'Enter your current PIN');return;}
  if(!/^\d{8,}$/.test(n)){setMsg(m,'New PIN must be 8 or more digits');return;}
  const r=await post('/api/me/change-pin',{old_pin:o,new_pin:n});
  $('cOld').value='';$('cNew').value='';
  if(r.d&&r.d.ok){setMsg(m,'PIN changed',true);}else{setMsg(m,(r.d&&r.d.error)||'Could not change PIN');}
};

let phraseTimer=null;
function hidePhrase(){$('phrase').textContent='';if(phraseTimer){clearTimeout(phraseTimer);phraseTimer=null;}}
$('bkBtn').onclick=async function(){
  const m=$('bMsg');setMsg(m,'');hidePhrase();
  const pin=fixDigits($('bPin').value);
  if(!pin){setMsg(m,'Enter your PIN');return;}
  const r=await post('/api/me/backup',{pin:pin});
  $('bPin').value='';
  if(r.d&&r.d.ok){
    const d=document.createElement('div');d.className='phrase mono';d.textContent=r.d.mnemonic;
    $('phrase').appendChild(d);setMsg(m,'Hidden automatically in 60 seconds',true);
    phraseTimer=setTimeout(function(){hidePhrase();setMsg(m,'');},60000);
  }else{setMsg(m,(r.d&&r.d.error)||'Could not show phrase');}
};

setInterval(function(){if(ME&&document.visibilityState==='visible')refresh();},30000);
boot();
</script></body></html>)ME";

}  // namespace caesar
