// Copyright (c) Caesar contributors. See LICENSE.
//
// Extras layered on top of me_page.hpp (the /me wallet page) without rewriting it:
//  - czr_me_page_html(): kCzrMePage + offline-safe fetch layer + forge hammer + greeting + dua + PWA install
//  - kCzrInvitePage: invite page that logs the new user in right after registering
//  - czr_register_extra(): /manifest.webmanifest, /icon.svg, /sw.js
#pragma once
#include <string>
#include "me_page.hpp"
#include "me_boost.hpp"

namespace caesar {

inline const char* const kCzrMeHead = R"HEAD(<link rel="manifest" href="/manifest.webmanifest">
<link rel="icon" href="/icon.svg">
<meta name="mobile-web-app-capable" content="yes">
<style>
#czrOff{display:none;position:fixed;top:0;left:0;right:0;z-index:50;background:#6b4a1a;color:#ffd48a;font-size:13px;text-align:center;padding:6px}
.czr-hero{position:relative;overflow:hidden;background:radial-gradient(120% 90% at 50% 100%,#3a2412,var(--card) 70%)}
.czr-greet{font:700 21px/1.3 Georgia,serif;color:var(--gold);text-align:center;unicode-bidi:plaintext}
.czr-sub{font-size:13px;color:var(--mut);text-align:center;margin-top:2px;unicode-bidi:plaintext}
.czr-hero svg{width:100%;height:150px;display:block;margin-top:6px;overflow:visible}
#czrSp{position:absolute;left:50%;top:100px;width:0;height:0}
#czrSp i{position:absolute;width:4px;height:4px;border-radius:50%;background:#ffd27a;box-shadow:0 0 6px 2px #ff7a2f}
.czr-coin{position:fixed;z-index:60;width:30px;height:30px;margin:-15px;border-radius:50%;display:grid;place-items:center;font:700 15px Georgia,serif;color:#5a3a06;background:radial-gradient(circle at 35% 30%,#fff1b8,#f0c040 55%,#a8761a);box-shadow:0 0 12px rgba(240,192,64,.8);pointer-events:none}
.czr-dua{background:#1b1710;border:1px solid #6b4a1a;text-align:center;font:18px/1.9 Georgia,serif;color:#f3ead7;unicode-bidi:plaintext}
.czr-dua button{width:auto;padding:8px 22px;margin-top:8px;font-size:14px}
#bal{transition:color .4s,text-shadow .4s}
#bal.czr-up{color:var(--green)!important;text-shadow:0 0 18px rgba(77,255,146,.5)}
#bal.czr-dn{color:var(--red)!important;text-shadow:0 0 18px rgba(255,92,122,.5)}
</style>
<script>
(function(){
var F=window.fetch.bind(window),CA=/\/api\/me\/(info|balance|txs|network)$/;
function ls(k,v){try{if(v===null)localStorage.removeItem(k);else if(v===undefined)return localStorage.getItem(k);else localStorage.setItem(k,v);}catch(e){}return null;}
function wipe(){['info','balance','txs','network'].forEach(function(n){ls('czr_c:/api/me/'+n,null);});}
function banner(on){window.__czrOffline=on;var el=document.getElementById('czrOff');if(el)el.style.display=on?'block':'none';}
window.fetch=async function(u,o){
  var url=typeof u==='string'?u:(u&&u.url)||'',p=url.split('?')[0],m=((o&&o.method)||'GET').toUpperCase();
  try{
    var r=await F(u,o);
    if(m==='GET'&&CA.test(p)){
      banner(false);
      if(r.status===200){r.clone().text().then(function(t){ls('czr_c:'+p,t);});}
      else if(r.status===401){wipe();}
    }else if(m==='POST'&&/\/api\/me\/login$/.test(p)&&r.status===200){
      try{ls('czr_user',JSON.parse(o.body).username);}catch(e){}
    }else if(m==='POST'&&/\/api\/me\/logout$/.test(p)){wipe();}
    return r;
  }catch(e){
    if(m==='GET'&&CA.test(p)){
      var t=ls('czr_c:'+p);
      if(t){banner(true);return new Response(t,{status:200,headers:{'Content-Type':'application/json'}});}
    }
    return new Response('{"ok":false,"error":"offline"}',{status:503,headers:{'Content-Type':'application/json'}});
  }
};
})();
</script>)HEAD";

inline const char* const kCzrMeTail = R"TAIL(<script>
(function(){
var $=function(i){return document.getElementById(i);};
var nl=navigator.language||'en',lc=/^ar/i.test(nl)?'ar':/^fr/i.test(nl)?'fr':'en',reg=nl.split('-')[1]||'';
var T={
ar:{m:'صباح الخير',d:'مرحباً',e:'مساء الخير',n:'ليلة سعيدة',from:'أهلاً بك من',off:'غير متصل · هذه آخر بيانات محفوظة',inst:'ثبّت المحفظة على هاتفك',ios:'للتثبيت: زر المشاركة ثم «إضافة إلى الشاشة الرئيسية»',snd:'الصوت',own:'أنت من أوائل المؤسسين',amen:'آمين'},
fr:{m:'Bonjour',d:'Bonjour',e:'Bonsoir',n:'Bonne nuit',from:'Bienvenue depuis',off:'Hors ligne · dernières données enregistrées',inst:'Installer le portefeuille',ios:'Pour installer : Partager, puis « Sur l’écran d’accueil »',snd:'Son',own:'Vous êtes parmi les premiers fondateurs',amen:'Amen'},
en:{m:'Good morning',d:'Hello',e:'Good evening',n:'Good night',from:'Welcome from',off:'Offline · showing last saved data',inst:'Install the wallet',ios:'To install: tap Share, then “Add to Home Screen”',snd:'Sound',own:'You are among the first founders',amen:'Amen'}};
var L=T[lc],H=new Date().getHours();
var rm=window.matchMedia&&matchMedia('(prefers-reduced-motion: reduce)').matches;
var snd=localStorage.getItem('czr_snd')==='1',ac=null,busy=false,prev=null,seen=false,dip=null;
function sleep(ms){return new Promise(function(r){setTimeout(r,ms);});}

var off=document.createElement('div');off.id='czrOff';off.textContent=L.off;
document.body.insertBefore(off,document.body.firstChild);
if(window.__czrOffline)off.style.display='block';

var tab=$('t_wallet');
var hero=document.createElement('div');hero.className='card czr-hero';
hero.innerHTML='<div class="czr-greet" id="czrGreet"></div><div class="czr-sub" id="czrSub"></div>'+
'<div style="position:relative"><svg viewBox="0 0 300 150" aria-hidden="true">'+
'<ellipse id="czrGlow" cx="150" cy="100" rx="95" ry="38" fill="#ff7a2f" opacity="0"/>'+
'<path d="M80 100H220V114H196L190 130H110L104 114H90Q78 112 70 104Z" fill="#3b3631"/>'+
'<rect x="80" y="98" width="140" height="4" rx="2" fill="#8d8378"/>'+
'<rect x="104" y="130" width="92" height="14" rx="3" fill="#2a2420"/>'+
'<g id="czrHam" style="transform-origin:262px 46px">'+
'<line x1="148" y1="89" x2="262" y2="46" stroke="#8a5a34" stroke-width="8" stroke-linecap="round"/>'+
'<rect x="112" y="76" width="46" height="26" rx="4" fill="#9aa1a8"/>'+
'<rect x="112" y="76" width="46" height="5" rx="2" fill="#e5eaee" opacity=".6"/>'+
'<rect x="152" y="76" width="6" height="26" fill="#f0c040"/></g></svg><div id="czrSp"></div></div>'+
'<div class="row" style="margin-top:6px"><button class="alt sm2" id="czrSnd"></button><button class="sm2" id="czrInst" style="display:none"></button></div>'+
'<div class="czr-sub" id="czrIos" style="display:none"></div>';
tab.insertBefore(hero,tab.firstChild);

var dk='czr_dua_'+new Date().toDateString()+(H<12?'a':H<20?'p':'n');
var dua=H>=4&&H<12?'أصبحنا وأصبح الملك لله، والحمد لله':(H>=15&&H<20)?'أمسينا وأمسى الملك لله، والحمد لله':(H>=21||H<4)?'باسمك اللهم أموت وأحيا':'';
if(lc==='ar'&&dua&&!localStorage.getItem(dk)){
  var dc=document.createElement('div');dc.className='card czr-dua';
  var dt=document.createElement('div');dt.textContent=dua;
  var db=document.createElement('button');db.textContent=L.amen;
  db.onclick=function(){try{localStorage.setItem(dk,'1');}catch(e){}dc.remove();};
  dc.appendChild(dt);dc.appendChild(db);tab.insertBefore(dc,hero.nextSibling);
}

function A(){if(!ac)ac=new(window.AudioContext||window.webkitAudioContext)();if(ac.state==='suspended')ac.resume();return ac;}
function clang(){
  if(!snd)return;var c=A(),t=c.currentTime,g=c.createGain();
  g.gain.setValueAtTime(.45,t);g.gain.exponentialRampToValueAtTime(.001,t+1.3);g.connect(c.destination);
  [520,833,1270,2100].forEach(function(f,i){var o=c.createOscillator(),og=c.createGain();o.type=i?'sine':'triangle';o.frequency.value=f;og.gain.value=1/(i+1);o.connect(og);og.connect(g);o.start(t);o.stop(t+1.3);});
}
function sndLabel(){$('czrSnd').textContent=(snd?'🔊 ':'🔇 ')+L.snd;}
sndLabel();
$('czrSnd').onclick=function(){snd=!snd;try{localStorage.setItem('czr_snd',snd?'1':'0');}catch(e){}sndLabel();if(snd){A();clang();}};

function sparks(){
  var box=$('czrSp');
  for(var i=0;i<12;i++){
    var s=document.createElement('i');box.appendChild(s);
    var a=(-20-Math.random()*140)*Math.PI/180,d=40+Math.random()*80,x=Math.cos(a)*d,y=Math.sin(a)*d;
    s.animate([{transform:'translate(0,0)',opacity:1},{transform:'translate('+x+'px,'+y+'px)',opacity:1,offset:.55},{transform:'translate('+x*1.2+'px,'+(y+70)+'px) scale(.2)',opacity:0}],{duration:1500+Math.random()*700,easing:'cubic-bezier(.1,.7,.4,1)'}).onfinish=(function(n){return function(){n.remove();};})(s);
  }
  $('czrGlow').animate([{opacity:.85},{opacity:0}],{duration:1400,easing:'ease-out'});
}
function coin(){
  var s=$('czrSp').getBoundingClientRect(),t=$('bal').getBoundingClientRect(),c=document.createElement('b');
  c.className='czr-coin';c.textContent='C';c.style.left=s.left+'px';c.style.top=s.top+'px';document.body.appendChild(c);
  var dx=t.left+t.width/2-s.left,dy=t.top+t.height/2-s.top;
  c.animate([{transform:'translate(0,0) scale(.4)',opacity:1},{transform:'translate('+dx*.3+'px,'+(dy*.5-50)+'px) scale(1.2)',opacity:1,offset:.45},{transform:'translate('+dx+'px,'+dy+'px) scale(.5)',opacity:0}],{duration:1300,easing:'cubic-bezier(.3,.6,.3,1)'}).onfinish=function(){c.remove();};
}
function play(a,b,ms,ez){
  var an=$('czrHam').animate([{transform:'rotate('+a+'deg)'},{transform:'rotate('+b+'deg)'}],{duration:ms,easing:ez,fill:'forwards'});
  if(prev)prev.cancel();prev=an;return an.finished;
}
async function strike(real){
  if(rm||busy)return;busy=true;
  try{
    await play(0,40,1100,'cubic-bezier(.3,.7,.3,1)');await sleep(150);
    await play(40,0,300,'cubic-bezier(.6,0,1,.6)');
    sparks();if(real){coin();clang();}
  }catch(e){}
  busy=false;
}
function flash(up){
  var b=$('bal');b.classList.remove('czr-up','czr-dn');void b.offsetWidth;b.classList.add(up?'czr-up':'czr-dn');
  setTimeout(function(){b.classList.remove('czr-up','czr-dn');},1500);
}
function count(a,b){
  if(rm)return;var t0=performance.now();
  (function f(t){var k=Math.min(1,(t-t0)/900),e=1-Math.pow(1-k,3);$('bal').textContent=fmt(Math.round(a+(b-a)*e));if(k<1)requestAnimationFrame(f);})(t0);
}
var _lb=loadBalance;
loadBalance=async function(){
  var old=BAL,ok=await _lb();
  if(!ok)return ok;
  if(!seen){seen=true;return ok;}
  if(BAL!==old){count(old,BAL);flash(BAL>old);if(BAL>old)strike(true);}
  return ok;
};

function greet(){
  var tod=H>=5&&H<12?L.m:H<17?L.d:H<22?L.e:L.n;
  $('czrGreet').textContent=tod+' '+ME.username;
  var sub='';
  if(/^[A-Za-z]{2}$/.test(reg)){
    var cc=reg.toUpperCase(),flag=String.fromCodePoint.apply(null,cc.split('').map(function(c){return 127397+c.charCodeAt(0);})),name=cc;
    try{name=new Intl.DisplayNames([lc],{type:'region'}).of(cc)||cc;}catch(e){}
    sub=flag+' '+L.from+' '+name;
  }
  if(ME.is_founder)sub+=(sub?' · ':'')+L.own;
  $('czrSub').textContent=sub;
  setTimeout(function(){strike(false);},700);
}
var tries=0,iv=setInterval(function(){
  if(ME){clearInterval(iv);greet();}else if(++tries>80){clearInterval(iv);}
},250);

var u=$('u'),sv=localStorage.getItem('czr_user');
if(u&&!u.value&&sv)u.value=sv;

var bi=$('czrInst'),ios=$('czrIos');
bi.textContent=L.inst;
window.addEventListener('beforeinstallprompt',function(e){e.preventDefault();dip=e;bi.style.display='';});
bi.onclick=function(){if(dip){dip.prompt();dip=null;bi.style.display='none';}};
if(/iphone|ipad/i.test(navigator.userAgent)&&!(window.matchMedia&&matchMedia('(display-mode: standalone)').matches)){ios.textContent=L.ios;ios.style.display='block';}
if('serviceWorker' in navigator&&location.protocol==='https:'){navigator.serviceWorker.register('/sw.js',{scope:'/me'}).catch(function(){});}
})();
</script>)TAIL";

inline const char* const kCzrManifest = R"MANI({"name":"Caesar Wallet","short_name":"Caesar","start_url":"/me","scope":"/","display":"standalone","background_color":"#0a0e17","theme_color":"#0a0e17","icons":[{"src":"/icon.svg","sizes":"any","type":"image/svg+xml","purpose":"any"}]})MANI";

inline const char* const kCzrIcon = R"ICON(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512"><rect width="512" height="512" rx="112" fill="#0a0e17"/><rect x="40" y="40" width="432" height="432" rx="86" fill="#f0c040"/><path d="M290 90 160 290h92l-30 132 130-200h-94z" fill="#0a0e17"/></svg>)ICON";

inline const char* const kCzrSw = R"SW(var C='czr-me-v1';
self.addEventListener('install',function(e){e.waitUntil(caches.open(C).then(function(c){return c.add('/me');}).catch(function(){}));self.skipWaiting();});
self.addEventListener('activate',function(e){e.waitUntil(self.clients.claim());});
self.addEventListener('fetch',function(e){
  var r=e.request;if(r.mode!=='navigate')return;
  if(new URL(r.url).pathname!=='/me')return;
  e.respondWith(fetch(r).then(function(x){if(x.ok){var k=x.clone();caches.open(C).then(function(c){c.put('/me',k);});}return x;}).catch(function(){return caches.match('/me');}));
});)SW";

inline const std::string& czr_me_page_html() {
    static const std::string html = [] {
        std::string h = kCzrMePage;
        const auto b = h.rfind("</body>");
        if (b != std::string::npos) h.insert(b, std::string(kCzrMeTail) + kCzrMeBoost + kCzrMeTerms + kCzrMeMiner);
        const auto p = h.find("</head>");
        if (p != std::string::npos) h.insert(p, kCzrMeHead);
        return h;
    }();
    return html;
}

template <class Server>
inline void czr_register_extra(Server& server) {
    server.Get("/manifest.webmanifest", [](const auto&, auto& res) {
        res.set_header("Cache-Control", "no-cache");
        res.set_content(kCzrManifest, "application/manifest+json");
    });
    server.Get("/icon.svg", [](const auto&, auto& res) {
        res.set_header("Cache-Control", "public, max-age=86400");
        res.set_content(kCzrIcon, "image/svg+xml");
    });
    czr_register_boost(server);
    server.Get("/sw.js", [](const auto&, auto& res) {
        res.set_header("Cache-Control", "no-cache");
        res.set_header("Service-Worker-Allowed", "/");
        res.set_content(kCzrSw, "application/javascript");
    });
}

inline const char* const kCzrInvitePage = R"INVITE(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Caesar</title>
<style>
body{background:radial-gradient(120% 60% at 50% 0,#33200f,#080b12 62%);color:#eef1f6;font-family:system-ui,Roboto,sans-serif;display:flex;align-items:center;justify-content:center;min-height:100vh;margin:0;padding:20px}
.card{background:#181e2c;border:1px solid #f0c040;border-radius:16px;padding:24px;width:100%;max-width:340px;text-align:center}
h2{color:#f0c040;margin:0 0 6px;font-family:Georgia,serif}
p{color:#8892a4;font-size:14px;margin:6px 0}
input{background:#000;color:#fff;border:1px solid #f0c040;border-radius:10px;padding:13px;width:100%;margin:8px 0;font-size:16px;text-align:center;box-sizing:border-box}
button{background:#f0c040;color:#000;border:none;padding:14px;width:100%;font-size:16px;font-weight:bold;border-radius:10px;margin-top:12px;cursor:pointer}
button.alt{background:#1a2440;color:#fff;border:1px solid #33406a}
.msg{font-size:13px;margin-top:12px;min-height:20px;color:#ff5c7a}
.mn{background:#000;border:1px dashed #f0c040;border-radius:12px;padding:12px;margin:10px 0;color:#f0c040;font-family:ui-monospace,Menlo,monospace;font-size:15px;line-height:1.8;word-break:break-word;direction:ltr}
.ad{font-family:ui-monospace,Menlo,monospace;font-size:11px;color:#4dff92;word-break:break-all;direction:ltr}
</style>
</head>
<body>
<div class="card">
  <h2 id="tt"></h2>
  <div id="form">
    <p id="hint"></p>
    <input id="username" type="text" maxlength="32" autocapitalize="none" autocomplete="username">
    <input id="pin" type="password" inputmode="numeric" maxlength="12" autocomplete="new-password">
    <button id="go"></button>
    <div id="msg" class="msg"></div>
    <button class="alt" id="signin" style="display:none"></button>
  </div>
  <div id="done" style="display:none">
    <p id="okt" style="color:#4dff92"></p>
    <div class="ad" id="addr"></div>
    <p id="warn" style="color:#ffd48a"></p>
    <div class="mn" id="mn"></div>
    <button class="alt" id="cp"></button>
    <button id="saved"></button>
  </div>
</div>
<script>
var nl=navigator.language||'en',lc=/^ar/i.test(nl)?'ar':/^fr/i.test(nl)?'fr':'en';
var T={
ar:{tt:'انضم إلى قيصر',hint:'اختر اسماً ورقماً سرياً من 8 أرقام لإنشاء محفظتك.',un:'اسم المستخدم (3 أحرف أو أكثر)',pn:'رقم سري من 8 أرقام',go:'إنشاء المحفظة',short:'الاسم يجب أن يكون 3 أحرف على الأقل',pshort:'الرقم السري يجب أن يكون 8 أرقام على الأقل',bad:'رابط الدعوة غير صالح',busy:'جارٍ إنشاء المحفظة...',taken:'هذا الاسم محجوز. إن كان اسمك فمحفظتك موجودة، فاضغط تسجيل الدخول. وإلا فاختر اسماً آخر.',signin:'لدي محفظة، تسجيل الدخول',fail:'فشل التسجيل',net:'خطأ في الشبكة: ',ok:'تم إنشاء محفظتك. عنوانك:',warn:'احفظ هذه الكلمات الـ12 على ورقة الآن. هي الطريقة الوحيدة لاستعادة محفظتك ولن تظهر مرة أخرى.',cp:'نسخ الكلمات',copied:'تم النسخ',saved:'حفظتها، افتح محفظتي'},
fr:{tt:'Rejoindre Caesar',hint:'Choisissez un nom et un code PIN à 8 chiffres pour créer votre portefeuille.',un:'Nom d’utilisateur (3+ caractères)',pn:'Code PIN à 8 chiffres',go:'CRÉER LE PORTEFEUILLE',short:'Le nom doit avoir au moins 3 caractères',pshort:'Le PIN doit avoir au moins 8 chiffres',bad:'Lien d’invitation invalide',busy:'Création du portefeuille...',taken:'Ce nom est pris. S’il est à vous, votre portefeuille existe déjà : appuyez sur Se connecter. Sinon, choisissez un autre nom.',signin:'J’ai déjà un portefeuille, me connecter',fail:'Échec de l’inscription',net:'Erreur réseau : ',ok:'Portefeuille créé. Votre adresse :',warn:'Notez ces 12 mots sur papier maintenant. C’est le seul moyen de restaurer votre portefeuille et ils ne s’afficheront plus.',cp:'Copier les mots',copied:'Copié',saved:'Je les ai notés, ouvrir mon portefeuille'},
en:{tt:'Join Caesar',hint:'Choose a username and an 8-digit PIN to create your wallet.',un:'Username (3+ chars)',pn:'8-digit PIN',go:'CREATE WALLET',short:'Username must be 3+ chars',pshort:'PIN must be 8+ digits',bad:'Invalid invite link',busy:'Creating wallet...',taken:'This username is taken. If it is yours, your wallet already exists: tap Sign in. Otherwise choose another name.',signin:'I already have a wallet, sign in',fail:'Registration failed',net:'Network error: ',ok:'Wallet created. Your address:',warn:'Write these 12 words on paper now. They are the only way to restore your wallet and will not be shown again.',cp:'Copy words',copied:'Copied',saved:'I saved them, open my wallet'}};
var L=T[lc];
if(lc==='ar')document.documentElement.dir='rtl';
document.documentElement.lang=lc;
function $(i){return document.getElementById(i);}
function fixDigits(s){return String(s||'').replace(/[٠-٩]/g,function(c){return String(c.charCodeAt(0)-1632);}).replace(/[۰-۹]/g,function(c){return String(c.charCodeAt(0)-1776);});}
$('tt').textContent=L.tt;$('hint').textContent=L.hint;$('go').textContent=L.go;
$('username').placeholder=L.un;$('pin').placeholder=L.pn;$('signin').textContent=L.signin;
$('okt').textContent=L.ok;$('warn').textContent=L.warn;$('cp').textContent=L.cp;$('saved').textContent=L.saved;
$('signin').onclick=function(){location.href='/me';};
$('saved').onclick=function(){location.href='/me';};
$('cp').onclick=function(){
  var t=$('mn').textContent,b=$('cp');
  if(navigator.clipboard&&navigator.clipboard.writeText){navigator.clipboard.writeText(t).then(function(){b.textContent=L.copied;});}
};
(async function(){try{var r=await fetch('/api/me/info',{credentials:'same-origin',cache:'no-store'});if(r.status===200){location.replace('/me');}}catch(e){}})();
function getRef(){return new URLSearchParams(location.search).get('ref')||'';}
async function register(){
  var username=$('username').value.trim(),pin=fixDigits($('pin').value.trim()),msg=$('msg'),ref=getRef();
  $('signin').style.display='none';msg.style.color='#ff5c7a';
  if(username.length<3){msg.textContent=L.short;return;}
  if(pin.length<8){msg.textContent=L.pshort;return;}
  if(!ref){msg.textContent=L.bad;return;}
  msg.style.color='#f0c040';msg.textContent=L.busy;$('go').disabled=true;
  try{
    var r=await fetch('/api/user/register',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({username:username,pin:pin,parent:ref})});
    var d=await r.json();
    if(d&&d.ok){
      try{
        var lr=await fetch('/api/me/login',{method:'POST',credentials:'same-origin',headers:{'Content-Type':'application/json'},body:JSON.stringify({username:username,pin:pin})});
        if(lr.status===200){try{localStorage.setItem('czr_user',username);}catch(e){}}
      }catch(e){}
      $('form').style.display='none';$('done').style.display='block';
      $('addr').textContent=d.address||'';$('mn').textContent=d.mnemonic||'';
    }else{
      msg.style.color='#ff5c7a';
      if(d&&d.error&&d.error.indexOf('already taken')>=0){msg.textContent=L.taken;$('signin').style.display='block';}
      else{msg.textContent=(d&&d.error)||L.fail;}
    }
  }catch(e){msg.style.color='#ff5c7a';msg.textContent=L.net+e.message;}
  $('go').disabled=false;
}
$('go').onclick=register;
</script>
</body>
</html>)INVITE";

}  // namespace caesar
