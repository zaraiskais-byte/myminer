// Copyright (c) Caesar contributors. See LICENSE.
// Live mining card + auto-update layer for the /me wallet page.
#pragma once
#include <string>

namespace caesar {

inline std::string czr_build_id() { return std::string(__DATE__ " " __TIME__); }

template <class Server>
inline void czr_register_boost(Server& server) {
    server.Get("/api/czr/ver", [](const auto&, auto& res) {
        res.set_header("Cache-Control", "no-store");
        res.set_content(std::string("{\"v\":\"") + czr_build_id() + "\"}", "application/json");
    });
}

inline const char* const kCzrMeBoost = R"BOOST(<style>
.czr-live{margin:14px 0;border-radius:18px;padding:16px;position:relative;overflow:hidden;background:linear-gradient(160deg,#1a2440,#0f1522 60%);border:1px solid rgba(240,192,64,.35);box-shadow:0 0 28px rgba(240,192,64,.12)}
.czr-live:before{content:"";position:absolute;inset:-60%;background:conic-gradient(from 0deg,transparent,rgba(240,192,64,.18),transparent 30%);animation:czrSpin 6s linear infinite;pointer-events:none}
@keyframes czrSpin{to{transform:rotate(360deg)}}
.czr-live>*{position:relative}
.czr-top{display:flex;align-items:center;justify-content:space-between}
.czr-pill{display:inline-flex;gap:6px;align-items:center;font-size:12px;padding:4px 10px;border-radius:99px;background:rgba(0,0,0,.5);color:#8892a4}
.czr-pill i{width:8px;height:8px;border-radius:50%;background:#8892a4;display:inline-block}
.czr-on .czr-pill{color:#4dff92}
.czr-on .czr-pill i{background:#4dff92;animation:czrP 1.2s infinite}
@keyframes czrP{0%{box-shadow:0 0 0 0 rgba(77,255,146,.6)}100%{box-shadow:0 0 0 7px rgba(77,255,146,0)}}
.czr-hr{font:800 34px/1.1 Georgia,serif;color:#f0c040;margin:10px 0 2px;text-shadow:0 0 18px rgba(240,192,64,.35);direction:ltr;text-align:left}
.czr-eq{display:flex;gap:3px;align-items:flex-end;height:26px;direction:ltr}
.czr-eq b{width:5px;height:4px;border-radius:2px;background:linear-gradient(#f0c040,#ff7a2f);transition:height .35s}
.czr-grid{display:grid;grid-template-columns:repeat(4,1fr);gap:8px;margin-top:12px}
.czr-grid div{background:rgba(0,0,0,.4);border-radius:12px;padding:8px 4px;text-align:center}
.czr-grid b{display:block;font-size:17px;color:#fff;direction:ltr}
.czr-grid span{font-size:11px;color:#8892a4;unicode-bidi:plaintext}
#czrToast{position:fixed;top:12px;left:50%;transform:translateX(-50%);z-index:90;background:#f0c040;color:#000;font-weight:700;padding:10px 18px;border-radius:99px;box-shadow:0 4px 20px rgba(0,0,0,.5)}
</style>
<script>
(function(){
var nl=navigator.language||'en',lc=/^ar/i.test(nl)?'ar':/^fr/i.test(nl)?'fr':'en';
var T={
ar:{live:'\u064a\u0639\u0645\u0644 \u0627\u0644\u0622\u0646',idle:'\u0645\u062a\u0648\u0642\u0641',th:'\u062e\u064a\u0648\u0637',ht:'\u0627\u0644\u0627\u0631\u062a\u0641\u0627\u0639',pe:'\u0623\u0642\u0631\u0627\u0646',ag:'\u0645\u0646\u0630 \u0622\u062e\u0631 \u0628\u0644\u0648\u0643',upd:'\u062a\u0645 \u062a\u062d\u062f\u064a\u062b \u0627\u0644\u062a\u0637\u0628\u064a\u0642'},
fr:{live:'En marche',idle:'Arrete',th:'Threads',ht:'Hauteur',pe:'Pairs',ag:'Dernier bloc',upd:'Application mise a jour'},
en:{live:'Mining live',idle:'Idle',th:'Threads',ht:'Height',pe:'Peers',ag:'Last block',upd:'App updated'}};
var L=T[lc];
function $(i){return document.getElementById(i);}
function pick(o,re,d){d=d||0;if(!o||typeof o!=='object'||d>3)return;
  for(var k in o){if(re.test(k)){var v=o[k];if(Array.isArray(v))return v.length;
    if(typeof v==='number'||(typeof v==='string'&&v!==''&&isFinite(v)))return Number(v);}}
  for(var k2 in o){var r=pick(o[k2],re,d+1);if(r!==undefined)return r;}}
async function J(u){try{var r=await fetch(u,{credentials:'same-origin',cache:'no-store'});if(!r.ok)return null;return await r.json();}catch(e){return null;}}
function fh(h){if(h>=1e9)return (h/1e9).toFixed(2)+' GH/s';if(h>=1e6)return (h/1e6).toFixed(2)+' MH/s';if(h>=1e3)return (h/1e3).toFixed(2)+' kH/s';return Math.round(h)+' H/s';}
var RH=/^(height|block_?height|chain_?height|blocks)$/i,
    RP=/^(peers?|peer_?count|peers_?connected|connections)$/i,
    RT=/^(threads?|workers?|miners?|worker_?count|active_?workers|total_?threads)$/i,
    RR=/hash_?rate|hashes_?per|^hps$/i;
var lastH=null,lastChg=0,live=false,ver=null,box=null;
function mount(hero){
  box=document.createElement('div');box.className='czr-live';
  var bars='';for(var i=0;i<10;i++)bars+='<b></b>';
  box.innerHTML='<div class="czr-top"><span class="czr-pill"><i></i><em id="czrSt" style="font-style:normal">'+L.idle+'</em></span><div class="czr-eq" id="czrEq">'+bars+'</div></div>'
   +'<div class="czr-hr" id="czrHr">--</div>'
   +'<div class="czr-grid"><div><b id="czrTh">--</b><span>'+L.th+'</span></div><div><b id="czrHt">--</b><span>'+L.ht+'</span></div><div><b id="czrPe">--</b><span>'+L.pe+'</span></div><div><b id="czrAg">--</b><span>'+L.ag+'</span></div></div>';
  hero.parentNode.insertBefore(box,hero.nextSibling);
  tick();setInterval(tick,4000);
  setInterval(function(){
    var bs=document.querySelectorAll('#czrEq b');
    for(var i=0;i<bs.length;i++)bs[i].style.height=(live?(4+Math.random()*22):4)+'px';
    var a=$('czrAg');
    if(a){if(lastChg){var s=Math.max(0,Math.floor((Date.now()-lastChg)/1000));a.textContent=Math.floor(s/60)+':'+('0'+(s%60)).slice(-2);}else a.textContent='--';}
  },350);
  setInterval(function(){
    var m=$('czrHam');if(!live||!m||document.hidden)return;
    try{m.animate([{transform:'rotate(0deg)'},{transform:'rotate(40deg)',offset:.6},{transform:'rotate(0deg)'}],{duration:1000,easing:'ease-in-out'});}catch(e){}
  },2200);
}
async function tick(){
  if(document.hidden||!box)return;
  var n=await J('/api/me/network'),p=await J('/api/pool/stats');
  function g(re){var v=pick(n,re);return v===undefined?pick(p,re):v;}
  var h=g(RH),pe=g(RP),th=g(RT),hr=g(RR);
  if(h!==undefined){if(lastH!==null&&h!==lastH)lastChg=Date.now();lastH=h;$('czrHt').textContent=h;}
  if(pe!==undefined)$('czrPe').textContent=pe;
  if(th!==undefined)$('czrTh').textContent=th;
  $('czrHr').textContent=hr!==undefined?fh(hr):'--';
  live=(hr!==undefined&&hr>0)||(lastChg>0&&Date.now()-lastChg<150000);
  box.classList.toggle('czr-on',live);
  $('czrSt').textContent=live?L.live:L.idle;
}
async function chk(){
  if(document.hidden)return;
  var j=await J('/api/czr/ver');if(!j||!j.v)return;
  if(ver===null){ver=j.v;return;}
  if(j.v!==ver){
    var t=document.createElement('div');t.id='czrToast';t.textContent=L.upd;document.body.appendChild(t);
    ver=j.v;setTimeout(function(){location.replace('/me?v='+encodeURIComponent(j.v));},1300);
  }
}
chk();setInterval(chk,30000);
document.addEventListener('visibilitychange',function(){if(!document.hidden){chk();tick();}});
var n=0,w=setInterval(function(){var hero=document.querySelector('.czr-hero');if(hero||++n>120){clearInterval(w);if(hero)mount(hero);}},250);
})();
</script>)BOOST";

} // namespace caesar
