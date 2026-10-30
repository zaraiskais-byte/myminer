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
ar:{live:'\u064a\u0639\u0645\u0644 \u0627\u0644\u0622\u0646',idle:'\u0645\u062a\u0648\u0642\u0641',th:'\u0639\u0645\u0627\u0644 \u0646\u0634\u0637\u0648\u0646',ht:'\u0627\u0644\u0627\u0631\u062a\u0641\u0627\u0639',pe:'\u0623\u0642\u0631\u0627\u0646',ag:'\u0645\u0646\u0630 \u0622\u062e\u0631 \u0628\u0644\u0648\u0643',upd:'\u062a\u0645 \u062a\u062d\u062f\u064a\u062b \u0627\u0644\u062a\u0637\u0628\u064a\u0642'},
fr:{live:'En marche',idle:'Arrete',th:'Workers',ht:'Hauteur',pe:'Pairs',ag:'Dernier bloc',upd:'Application mise a jour'},
en:{live:'Mining live',idle:'Idle',th:'Workers',ht:'Height',pe:'Peers',ag:'Last block',upd:'App updated'}};
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
  var h=g(RH),pe=g(RP),th,hr;var ws=p?p.workers:null;if(Array.isArray(ws)){th=0;hr=0;ws.forEach(function(w){if(w.active){th++;hr+=Number(w.hashrate)||0;}});}else{th=g(RT);hr=g(RR);}
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

inline const char* const kCzrMeTerms = R"TERMS(<script>
(function(){
var nl=navigator.language||'en',lc=/^ar/i.test(nl)?'ar':/^fr/i.test(nl)?'fr':'en';
var T={
ar:{t:'\u0634\u0631\u0648\u0637 \u0627\u0644\u062a\u0639\u062f\u064a\u0646',fee:'\u0631\u0633\u0648\u0645 \u0627\u0644\u0645\u062c\u0645\u0639',rule:'\u0627\u0644\u0628\u0627\u0642\u064a \u064a\u0648\u0632\u0639 \u062d\u0633\u0628 \u0627\u0644\u0634\u064a\u0631\u0627\u062a',my:'\u0634\u064a\u0631\u0627\u062a\u0643',note:'\u0644\u0627 \u062a\u0648\u062c\u062f \u0648\u0639\u0648\u062f \u0628\u0645\u0628\u0644\u063a \u062b\u0627\u0628\u062a'},
fr:{t:'Conditions de minage',fee:'Frais du pool',rule:'Le reste est reparti selon les parts',my:'Vos parts',note:'Aucune promesse de montant fixe'},
en:{t:'Mining terms',fee:'Pool fee',rule:'The rest is split by shares',my:'Your shares',note:'No fixed-amount promises'}};
var L=T[lc],card=null,addr=null;
function $(i){return document.getElementById(i);}
function J(u){return fetch(u,{credentials:'same-origin',cache:'no-store'}).then(function(r){return r.ok?r.json():null;}).catch(function(){return null;});}
function fa(o,d){d=d||0;if(!o||d>4)return;if(typeof o==='string')return /^CZ1[0-9a-f]{40,}$/i.test(o)?o:undefined;if(typeof o!=='object')return;for(var k in o){var r=fa(o[k],d+1);if(r)return r;}}
function mount(anchor){
  card=document.createElement('div');
  card.style.cssText='margin:14px 0;border-radius:18px;padding:14px;background:rgba(0,0,0,.35);border:1px solid rgba(240,192,64,.25)';
  card.innerHTML='<div style="font:700 15px Georgia,serif;color:#f0c040;unicode-bidi:plaintext;margin-bottom:8px">'+L.t+'</div>'
   +'<div style="display:flex;justify-content:space-between;font-size:14px;color:#cfd6e4"><span>'+L.fee+'</span><b id="czrTFee" style="color:#fff;direction:ltr">--</b></div>'
   +'<div style="font-size:13px;color:#8892a4;margin:6px 0;unicode-bidi:plaintext">'+L.rule+' (<span id="czrTRest" style="direction:ltr;display:inline-block">--</span>)</div>'
   +'<div style="display:flex;justify-content:space-between;font-size:14px;color:#cfd6e4"><span>'+L.my+'</span><b id="czrTMy" style="color:#4dff92;direction:ltr">--</b></div>'
   +'<div style="font-size:12px;color:#8892a4;margin-top:8px;unicode-bidi:plaintext">'+L.note+'</div>';
  anchor.parentNode.insertBefore(card,anchor.nextSibling);
  tick();setInterval(tick,10000);
}
async function tick(){
  if(document.hidden||!card)return;
  var p=await J('/api/pool/stats');
  if(!addr){var i=await J('/api/me/info');if(i)addr=(typeof i.address==='string')?i.address:fa(i);}
  if(p&&p.fee_percent!==undefined){var f=Number(p.fee_percent);$('czrTFee').textContent=f+'%';$('czrTRest').textContent=(100-f)+'%';}
  var s=0;
  if(p&&Array.isArray(p.workers)&&addr){p.workers.forEach(function(w){if(w.address===addr)s+=Number(w.shares)||0;});}
  $('czrTMy').textContent=String(s);
}
var n=0,w=setInterval(function(){var a=document.querySelector('.czr-live');if(a||++n>160){clearInterval(w);if(a)mount(a);}},250);
})();
</script>)TERMS";

inline const char* const kCzrMeMiner = R"MINER(<script>
(function(){
var nl=navigator.language||'en',lc=/^ar/i.test(nl)?'ar':/^fr/i.test(nl)?'fr':'en';
var T={
ar:{t:'\u0627\u0644\u062a\u0639\u062f\u064a\u0646 \u0645\u0646 \u0647\u0627\u062a\u0641\u0643',go:'\u0627\u0628\u062f\u0623 \u0627\u0644\u062a\u0639\u062f\u064a\u0646',stop:'\u0623\u0648\u0642\u0641 \u0627\u0644\u062a\u0639\u062f\u064a\u0646',h:'\u0627\u0644\u0647\u0627\u0634',th:'\u062e\u064a\u0648\u0637',sh:'\u0634\u064a\u0631\u0627\u062a \u0648\u062c\u062f\u062a\u0647\u0627',n1:'\u064a\u0633\u062a\u0647\u0644\u0643 \u0627\u0644\u0645\u0639\u0627\u0644\u062c \u0648\u0627\u0644\u0628\u0637\u0627\u0631\u064a\u0629. \u0631\u0633\u0648\u0645 \u0627\u0644\u0645\u062c\u0645\u0639',n2:'. \u0623\u0648\u0642\u0641 \u0645\u062a\u0649 \u0634\u0626\u062a.',q:'\u0647\u0644 \u062a\u0631\u064a\u062f \u0627\u0644\u0628\u062f\u0621\u061f'},
fr:{t:'Miner depuis votre telephone',go:'Demarrer',stop:'Arreter',h:'Hash',th:'Threads',sh:'Parts trouvees',n1:'Utilise le processeur et la batterie. Frais du pool',n2:'. Arretez quand vous voulez.',q:'Demarrer ?'},
en:{t:'Mine from your phone',go:'Start mining',stop:'Stop mining',h:'Hash',th:'Threads',sh:'Shares found',n1:'Uses CPU and battery. Pool fee',n2:'. Stop any time.',q:'Start?'}};
var L=T[lc];
function W(){
  var N=65536,mH=new Uint32Array(N),mL=new Uint32Array(N),gH=new Uint32Array(N),gL=new Uint32Array(N);
  var K=new Uint32Array(64),H0=new Uint32Array(8);
  (function(){var n=0,c=2;while(n<64){var p=true;for(var d=2;d*d<=c;d++){if(c%d===0){p=false;break;}}if(p){if(n<8)H0[n]=Math.floor((Math.sqrt(c)%1)*4294967296);K[n]=Math.floor((Math.cbrt(c)%1)*4294967296);n++;}c++;}})();
  function sha(b){
    var l=b.length,nb=Math.ceil((l+9)/64)*64,m=new Uint8Array(nb);m.set(b);m[l]=128;
    var bl=l*8;m[nb-4]=(bl>>>24)&255;m[nb-3]=(bl>>>16)&255;m[nb-2]=(bl>>>8)&255;m[nb-1]=bl&255;
    var h=new Uint32Array(H0),w=new Uint32Array(64),i;
    for(var o=0;o<nb;o+=64){
      for(i=0;i<16;i++)w[i]=(m[o+4*i]<<24)|(m[o+4*i+1]<<16)|(m[o+4*i+2]<<8)|m[o+4*i+3];
      for(i=16;i<64;i++){var a=w[i-15],c=w[i-2];
        var s0=((a>>>7)|(a<<25))^((a>>>18)|(a<<14))^(a>>>3);
        var s1=((c>>>17)|(c<<15))^((c>>>19)|(c<<13))^(c>>>10);
        w[i]=(w[i-16]+s0+w[i-7]+s1)|0;}
      var A=h[0],B=h[1],C=h[2],D=h[3],E=h[4],F=h[5],G=h[6],Q=h[7];
      for(i=0;i<64;i++){
        var S1=((E>>>6)|(E<<26))^((E>>>11)|(E<<21))^((E>>>25)|(E<<7));
        var ch=(E&F)^(~E&G);
        var t1=(Q+S1+ch+K[i]+w[i])|0;
        var S0=((A>>>2)|(A<<30))^((A>>>13)|(A<<19))^((A>>>22)|(A<<10));
        var mj=(A&B)^(A&C)^(B&C);
        var t2=(S0+mj)|0;
        Q=G;G=F;F=E;E=(D+t1)|0;D=C;C=B;B=A;A=(t1+t2)|0;}
      h[0]=(h[0]+A)|0;h[1]=(h[1]+B)|0;h[2]=(h[2]+C)|0;h[3]=(h[3]+D)|0;h[4]=(h[4]+E)|0;h[5]=(h[5]+F)|0;h[6]=(h[6]+G)|0;h[7]=(h[7]+Q)|0;
    }
    var out=new Uint8Array(32);
    for(i=0;i<8;i++){out[4*i]=(h[i]>>>24)&255;out[4*i+1]=(h[i]>>>16)&255;out[4*i+2]=(h[i]>>>8)&255;out[4*i+3]=h[i]&255;}
    return out;
  }
  var H=0,L=0;
  function mulC(ch,cl){
    var a0=L&65535,a1=L>>>16,b0=cl&65535,b1=cl>>>16;
    var p00=a0*b0,p01=a0*b1,p10=a1*b0,p11=a1*b1;
    var mid=p01+p10+(p00>>>16);
    var lo=(((mid&65535)<<16)|(p00&65535))>>>0;
    var hi=(p11+Math.floor(mid/65536))>>>0;
    H=(hi+Math.imul(L,ch)+Math.imul(H,cl))>>>0;L=lo;
  }
  function mix(){
    var t=((L>>>30)|(H<<2))>>>0;L=(L^t)>>>0;H=(H^(H>>>30))>>>0;
    mulC(0xbf58476d,0x1ce4e5b9);
    t=((L>>>27)|(H<<5))>>>0;L=(L^t)>>>0;H=(H^(H>>>27))>>>0;
    mulC(0x94d049bb,0x133111eb);
    t=((L>>>31)|(H<<1))>>>0;L=(L^t)>>>0;H=(H^(H>>>31))>>>0;
  }
  var st=new Uint8Array(32);
  function ld(o){
    L=(st[o&31]|(st[(o+1)&31]<<8)|(st[(o+2)&31]<<16)|(st[(o+3)&31]<<24))>>>0;
    H=(st[(o+4)&31]|(st[(o+5)&31]<<8)|(st[(o+6)&31]<<16)|(st[(o+7)&31]<<24))>>>0;
  }
  function put(b,o,h,l){
    b[o]=l&255;b[o+1]=(l>>>8)&255;b[o+2]=(l>>>16)&255;b[o+3]=(l>>>24)&255;
    b[o+4]=h&255;b[o+5]=(h>>>8)&255;b[o+6]=(h>>>16)&255;b[o+7]=(h>>>24)&255;
  }
  for(var q=0;q<N;q++){H=0;L=q;mulC(0x9e3779b9,0x7f4a7c15);gH[q]=H;gL[q]=L;}
  function pow(hdr,nH,nL){
    var n=4+hdr.length+8,b=new Uint8Array(n);b[0]=2;b.set(hdr,4);put(b,4+hdr.length,nH,nL);
    st=sha(b);
    var pH=[0,0,0,0],pL=[0,0,0,0],k;
    for(k=0;k<4;k++){ld(k*8);pH[k]=H;pL[k]=L;}
    ld(0);var s0H=H,s0L=L;ld(8);
    var prH=(s0H^H^nH)>>>0,prL=(s0L^L^nL)>>>0,i;
    for(i=0;i<N;i++){
      k=(i&24)>>3;
      H=(prH^pH[k]^gH[i]^nH)>>>0;L=(prL^pL[k]^gL[i]^nL)>>>0;
      mix();mH[i]=H;mL[i]=L;prH=H;prL=L;
    }
    for(var r=0;r<4;r++){
      ld(r*8);var aH=H,aL=L;
      for(i=0;i<N;i++){
        H=(aH^mH[i]^nH)>>>0;L=(aL^mL[i]^nL^r)>>>0;mix();
        var idx=L&65535,oth=(idx^Math.imul(i,0x9e3779b9))&65535;
        H=(mH[idx]^mH[oth]^aH)>>>0;L=(mL[idx]^mL[oth]^aL^i)>>>0;mix();
        mH[idx]=H;mL[idx]=L;
        H=(aH^H)>>>0;L=(aL^L)>>>0;mix();
        aH=H;aL=L;
      }
      var rb=new Uint8Array(72);put(rb,0,aH,aL);
      for(var j=0;j<8;j++){
        var s=j*8,v;
        if(s===0)v=aL&65535;else if(s<32)v=((aL>>>s)|(aH<<(32-s)))&65535;else v=(aH>>>(s-32))&65535;
        put(rb,8+8*j,mH[v],mL[v]);
      }
      st=sha(rb);
    }
    var fb=new Uint8Array(108);fb[0]=2;fb.set(st,4);put(fb,36,nH,nL);
    for(var j2=0;j2<8;j2++){var o2=j2*4,v2=st[o2&31]|(st[(o2+1)&31]<<8);put(fb,44+8*j2,mH[v2],mL[v2]);}
    return sha(fb);
  }
  function lz(d){var c=0;for(var i=0;i<32;i++){var x=d[i];if(x===0){c+=8;continue;}for(var bt=7;bt>=0;bt--){if((x&(1<<bt))===0)c++;else return c;}}return c;}
  var t0=sha(new Uint8Array([97,98,99]));
  if(t0[0]!==0xba||t0[1]!==0x78||t0[2]!==0x16||t0[3]!==0xbf){postMessage({t:'err',e:'sha256 self-test failed'});return;}
  var job=null,run=false,nH=1048576+Math.floor(Math.random()*1000000),nL=Math.floor(Math.random()*4294967296)>>>0;
  function step(){
    if(!run||!job)return;
    var d=pow(job.hdr,nH,nL),z=lz(d);
    if(z>=job.sd)postMessage({t:'share',id:job.id,nonce:String(nH*4294967296+nL),lz:z});
    postMessage({t:'h'});
    nL=(nL+1)>>>0;if(nL===0)nH++;
    setTimeout(step,0);
  }
  onmessage=function(e){var m=e.data;
    if(m.t==='job'){job=m;if(!run){run=true;step();}}
    else if(m.t==='stop'){run=false;}
  };
}
function $(i){return document.getElementById(i);}
function fa(o,d){d=d||0;if(!o||d>4)return;if(typeof o==='string')return /^CZ1[0-9a-f]{40,}$/i.test(o)?o:undefined;if(typeof o!=='object')return;for(var k in o){var r=fa(o[k],d+1);if(r)return r;}}
var addr=null,ws=[],run=false,cur=null,found=0,hs=[],timers=[];
function hex2u8(h){var n=h.length>>1,u=new Uint8Array(n);for(var i=0;i<n;i++)u[i]=parseInt(h.substr(i*2,2),16);return u;}
function say(s){var e=$('czrMnD');if(e)e.textContent=s;}
async function getAddr(){
  if(addr)return addr;
  try{var r=await fetch('/api/me/info',{credentials:'same-origin',cache:'no-store'});if(r.ok){var i=await r.json();addr=(typeof i.address==='string')?i.address:fa(i);}}catch(e){}
  return addr;
}
async function fetchJob(){
  try{
    var r=await fetch('/api/pool/job?address='+encodeURIComponent(addr),{credentials:'same-origin',cache:'no-store'});
    var j=await r.json();
    if(!j||!j.ok||!j.header_hex)return;
    if(!cur||cur.id!==j.job_id){
      cur={id:j.job_id,hdr:hex2u8(j.header_hex),sd:j.share_difficulty};
      ws.forEach(function(w){w.postMessage({t:'job',id:cur.id,hdr:cur.hdr,sd:cur.sd});});
    }
  }catch(e){say('job: '+e);}
}
async function submit(m){
  found++;upd();
  try{
    var r=await fetch('/api/pool/submit',{method:'POST',credentials:'same-origin',headers:{'Content-Type':'application/json'},body:'{"address":"'+addr+'","job_id":'+m.id+',"nonce":'+m.nonce+'}'});
    var t=await r.text();say(r.status+' '+t.slice(0,90));
  }catch(e){say('submit: '+e);}
}
function hit(){var m=$('czrHam');if(!m)return;try{m.animate([{transform:'rotate(0deg)'},{transform:'rotate(40deg)',offset:.6},{transform:'rotate(0deg)'}],{duration:900,easing:'ease-in-out'});}catch(e){}}
function onmsg(e){var m=e.data;if(m.t==='h')hs.push(Date.now());else if(m.t==='share'){hit();submit(m);}else if(m.t==='err')say(m.e);}
function upd(){
  var now=Date.now();hs=hs.filter(function(x){return now-x<20000;});
  var r=hs.length/20,h=$('czrMnH'),s=$('czrMnS');
  if(h)h.textContent=run?((r<10?r.toFixed(1):Math.round(r))+' H/s'):'--';
  if(s)s.textContent=String(found);
}
function ui(){var b=$('czrMnB'),t=$('czrMnT');if(b)b.textContent=run?L.stop:L.go;if(t)t.disabled=run;}
function stop(){
  run=false;
  ws.forEach(function(w){try{w.postMessage({t:'stop'});w.terminate();}catch(e){}});
  ws=[];timers.forEach(function(x){clearInterval(x);});timers=[];ui();upd();
}
async function start(){
  if(run)return;
  var a=await getAddr();if(!a){say('no address');return;}
  var fe=($('czrTFee')||{}).textContent||'';
  var ok=true;
  try{if(!localStorage.getItem('czr_mn_ok')){ok=confirm(L.n1+' '+fe+L.n2+'\n'+L.q);if(ok)localStorage.setItem('czr_mn_ok','1');}}
  catch(e){ok=confirm(L.n1+' '+fe+L.n2+'\n'+L.q);}
  if(!ok)return;
  try{await fetch('/api/pool/register',{method:'POST',credentials:'same-origin',headers:{'Content-Type':'application/json'},body:'{"address":"'+a+'","worker":"web"}'});}catch(e){}
  var n=parseInt($('czrMnT').value,10)||1;
  var url=URL.createObjectURL(new Blob(['('+W.toString()+')()'],{type:'text/javascript'}));
  try{for(var i=0;i<n;i++){var w=new Worker(url);w.onmessage=onmsg;w.onerror=function(ev){say('worker error: '+((ev&&ev.message)||'blocked'));};ws.push(w);}}
  catch(e){say('worker: '+e);stop();return;}
  run=true;found=0;hs=[];cur=null;ui();
  await fetchJob();
  timers.push(setInterval(fetchJob,8000));timers.push(setInterval(upd,1000));timers.push(setInterval(hit,2500));
}
function mount(anchor){
  var c=document.createElement('div');
  c.style.cssText='margin:14px 0;border-radius:18px;padding:14px;background:rgba(0,0,0,.35);border:1px solid rgba(77,255,146,.25)';
  c.innerHTML='<div style="font:700 15px Georgia,serif;color:#4dff92;unicode-bidi:plaintext;margin-bottom:10px">'+L.t+'</div>'
   +'<div style="display:flex;gap:8px;align-items:center;margin-bottom:10px"><button id="czrMnB" style="flex:1;background:#f0c040;color:#000;border:none;padding:12px;border-radius:12px;font-weight:700;font-size:15px">'+L.go+'</button>'
   +'<select id="czrMnT" style="background:#000;color:#fff;border:1px solid #33406a;border-radius:10px;padding:10px"><option value="1">1</option><option value="2">2</option><option value="3">3</option><option value="4">4</option></select></div>'
   +'<div style="display:flex;justify-content:space-between;font-size:14px;color:#cfd6e4"><span>'+L.h+'</span><b id="czrMnH" style="color:#fff;direction:ltr">--</b></div>'
   +'<div style="display:flex;justify-content:space-between;font-size:14px;color:#cfd6e4;margin-top:4px"><span>'+L.sh+'</span><b id="czrMnS" style="color:#4dff92;direction:ltr">0</b></div>'
   +'<div id="czrMnD" style="font:11px monospace;color:#8892a4;margin-top:8px;direction:ltr;word-break:break-all"></div>';
  anchor.parentNode.insertBefore(c,anchor.nextSibling);
  $('czrMnB').onclick=function(){if(run){try{localStorage.setItem('czr_mn_run','0');}catch(e){}stop();}else start();};
  ui();
}
var wl=null;
function keepAwake(){try{if(navigator.wakeLock&&!wl){navigator.wakeLock.request('screen').then(function(x){wl=x;x.addEventListener('release',function(){wl=null;});}).catch(function(){wl=null;});}}catch(e){}}
document.addEventListener('visibilitychange',function(){if(document.visibilityState==='visible'&&run)keepAwake();});
setInterval(function(){try{if(run){keepAwake();localStorage.setItem('czr_mn_run','1');}}catch(e){}},5000);
var rt=setInterval(function(){if(!$('czrMnB'))return;clearInterval(rt);try{if(localStorage.getItem('czr_mn_run')==='1'&&localStorage.getItem('czr_mn_ok')&&!run)start();}catch(e){}},1500);
var tries=0,wt=setInterval(function(){
  var a=$('czrTFee');
  if(a){clearInterval(wt);mount(a.parentNode.parentNode);}
  else if(++tries>200){clearInterval(wt);}
},250);
})();
</script>)MINER";

} // namespace caesar
