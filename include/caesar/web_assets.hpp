#pragma once

inline const char* WALLET_HTML = R"HTML(
<!DOCTYPE html>

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



<link rel="icon" href="/icon-192.svg" type="image/svg+xml">

<title>Caesar CZR Wallet</title>

<style>

:root{

  --bg:#080b12;

  --bg-2:#0d1220;

  --surface:rgba(24,30,44,.72);

  --surface-solid:#181e2c;

  --surface-2:#1e2637;

  --border:rgba(255,255,255,.08);

  --border-2:rgba(255,255,255,.14);

  --text:#eef1f6;

  --dim:#8892a4;

  --dim-2:#5a6478;

  --gold:#ffcc4d;

  --gold-2:#e0a325;

  --cyan:#4de0ff;

  --cyan-2:#0099cc;

  --green:#4dff92;

  --red:#ff5c7a;

  --purple:#b370ff;

  --r:20px;

  --r-md:16px;

  --r-sm:12px;

  --shadow:0 20px 40px -20px rgba(0,0,0,.7);

  --shadow-gold:0 12px 32px -10px rgba(255,204,77,.35);

  --shadow-cyan:0 12px 32px -10px rgba(77,224,255,.3);

  --blur:backdrop-filter:blur(24px) saturate(180%);

}

*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}

html,body{margin:0;padding:0;height:100%;overflow:hidden;overscroll-behavior:none}

body{

  font-family:-apple-system,BlinkMacSystemFont,'SF Pro Display','Segoe UI',Roboto,system-ui,sans-serif;

  background:var(--bg);color:var(--text);

  -webkit-font-smoothing:antialiased;

  -webkit-text-size-adjust:100%;

  padding-top:env(safe-area-inset-top);

  padding-bottom:env(safe-area-inset-bottom);

  background-image:

    radial-gradient(ellipse at 20% 0%,rgba(255,204,77,.10),transparent 50%),

    radial-gradient(ellipse at 80% 100%,rgba(77,224,255,.08),transparent 55%);

}

button{font-family:inherit;cursor:pointer;border:none;color:inherit}

input,textarea{font-family:inherit}

a{color:var(--gold);text-decoration:none}

img{max-width:100%}



/* ===== Lock screen ===== */

#lock{

  position:fixed;inset:0;z-index:9999;

  display:flex;flex-direction:column;align-items:center;justify-content:center;

  padding:24px;

  background:

    radial-gradient(circle at 50% 20%,rgba(255,204,77,.15),transparent 40%),

    radial-gradient(circle at 50% 90%,rgba(77,224,255,.10),transparent 50%),

    var(--bg);

}

#lock .logo{

  width:108px;height:108px;border-radius:32px;

  background:linear-gradient(145deg,var(--gold),var(--gold-2));

  display:flex;align-items:center;justify-content:center;

  font-size:64px;font-weight:900;color:#0a0d12;

  box-shadow:0 20px 60px -10px rgba(255,204,77,.5),inset 0 2px 0 rgba(255,255,255,.4);

  margin-bottom:28px;

  animation:float 3s ease-in-out infinite;

}

@keyframes float{0%,100%{transform:translateY(0)}50%{transform:translateY(-8px)}}

#lock h1{color:var(--text);font-size:22px;margin:0 0 6px;font-weight:800;letter-spacing:2px}

#lock p{color:var(--dim);font-size:13px;margin:0 0 36px}

#lock input{

  background:rgba(0,0,0,.55);color:#fff;

  border:1.5px solid rgba(255,255,255,.12);

  border-radius:16px;padding:18px;font-size:26px;text-align:center;

  width:250px;letter-spacing:14px;margin-bottom:16px;

  transition:all .25s ease;

  box-shadow:inset 0 2px 8px rgba(0,0,0,.4);

}

#lock input:focus{

  outline:none;border-color:var(--gold);

  box-shadow:0 0 0 4px rgba(255,204,77,.15),inset 0 2px 8px rgba(0,0,0,.4);

}

#lock button{

  background:linear-gradient(145deg,var(--gold),var(--gold-2));

  color:#0a0d12;padding:18px 64px;font-size:16px;font-weight:800;

  border-radius:16px;letter-spacing:2px;

  box-shadow:var(--shadow-gold);

  transition:transform .12s ease;

}

#lock button:active{transform:scale(.95)}

#lock .msg{color:var(--red);font-size:13px;margin-top:18px;min-height:20px;text-align:center}



/* ===== App shell ===== */

#app{display:none;flex-direction:column;height:100vh;height:100dvh;position:relative}



header.app-header{

  position:relative;

  display:flex;align-items:center;justify-content:space-between;

  padding:16px 20px 14px;

  background:linear-gradient(180deg,rgba(13,18,32,.95),rgba(8,11,18,.6) 100%);

  -webkit-backdrop-filter:blur(20px);

  backdrop-filter:blur(20px);

  border-bottom:1px solid var(--border);

  flex-shrink:0;

  z-index:50;

}

.app-header .brand{display:flex;align-items:center;gap:12px}

.app-header .brand-icon{

  width:40px;height:40px;border-radius:12px;

  background:linear-gradient(145deg,var(--gold),var(--gold-2));

  display:flex;align-items:center;justify-content:center;

  font-size:22px;color:#0a0d12;font-weight:900;

  box-shadow:var(--shadow-gold),inset 0 1px 0 rgba(255,255,255,.5);

}

.app-header .brand-text{font-weight:800;font-size:18px;letter-spacing:-.3px}

.app-header .status{

  display:flex;align-items:center;gap:8px;

  background:rgba(255,255,255,.04);

  padding:8px 14px;border-radius:24px;

  font-size:12px;font-weight:600;color:var(--dim);

  border:1px solid var(--border);

  -webkit-backdrop-filter:blur(12px);

  backdrop-filter:blur(12px);

}

.app-header .dot{

  width:9px;height:9px;border-radius:50%;background:var(--green);

  box-shadow:0 0 12px var(--green);

  animation:pulse 2s ease-in-out infinite;

}

.app-header .dot.off{background:var(--red);box-shadow:0 0 12px var(--red)}

@keyframes pulse{0%,100%{opacity:1}50%{opacity:.5}}



main.content{

  flex:1;overflow-y:auto;overflow-x:hidden;

  padding:18px 18px 110px;

  -webkit-overflow-scrolling:touch;

  scroll-behavior:smooth;

}

.tab{display:none;animation:fadeUp .35s cubic-bezier(.16,1,.3,1)}

.tab.active{display:block}

@keyframes fadeUp{from{opacity:0;transform:translateY(12px)}to{opacity:1;transform:translateY(0)}}



/* ===== Cards - glass ===== */

.card{

  background:var(--surface);

  -webkit-backdrop-filter:blur(24px) saturate(180%);

  backdrop-filter:blur(24px) saturate(180%);

  border:1px solid var(--border);

  border-radius:var(--r);

  padding:18px;

  margin-bottom:16px;

  box-shadow:var(--shadow);

  transition:border-color .2s;

  position:relative;

  overflow:hidden;

}

.card::before{

  content:'';position:absolute;top:0;left:0;right:0;height:1px;

  background:linear-gradient(90deg,transparent,rgba(255,255,255,.15),transparent);

}

.card h2{

  color:var(--gold);font-size:12px;font-weight:800;

  margin:0 0 14px;letter-spacing:1.5px;text-transform:uppercase;

  display:flex;align-items:center;gap:8px;

}

.card h2::before{

  content:'';width:4px;height:14px;background:var(--gold);

  border-radius:2px;box-shadow:0 0 8px var(--gold);

}



/* ===== Balance card ===== */

.balance-card{

  background:linear-gradient(135deg,rgba(255,204,77,.08) 0%,rgba(77,224,255,.04) 100%),

             rgba(24,30,44,.85);

  -webkit-backdrop-filter:blur(24px) saturate(180%);

  backdrop-filter:blur(24px) saturate(180%);

  border:1px solid rgba(255,204,77,.2);

  position:relative;overflow:hidden;

  box-shadow:var(--shadow-gold),var(--shadow);

}

.balance-card::before{

  content:'';position:absolute;top:-50%;right:-30%;

  width:280px;height:280px;border-radius:50%;

  background:radial-gradient(circle,rgba(255,204,77,.25),transparent 65%);

  animation:glow 5s ease-in-out infinite;

}

@keyframes glow{0%,100%{opacity:.6}50%{opacity:1}}

.balance-card .label{

  color:var(--dim);font-size:11px;text-transform:uppercase;

  letter-spacing:2px;margin-bottom:8px;font-weight:700;

  position:relative;z-index:1;

}

.balance-card .value{

  color:var(--gold);font-size:38px;font-weight:900;

  letter-spacing:-1.5px;line-height:1.05;

  word-break:break-all;

  text-shadow:0 0 40px rgba(255,204,77,.5);

  position:relative;z-index:1;

  font-variant-numeric:tabular-nums;

}

.balance-card .unit{

  color:var(--dim);font-size:14px;margin-left:8px;font-weight:700;

  letter-spacing:1px;position:relative;z-index:1;

}



/* ===== QR / Address ===== */

.qr-wrap{

  background:#fff;padding:14px;border-radius:var(--r-md);

  display:flex;justify-content:center;margin:14px 0;

  box-shadow:0 8px 24px rgba(0,0,0,.4);

}

.qr-wrap img{width:230px;height:230px;display:block;border-radius:6px}

.addr{

  background:rgba(0,0,0,.5);padding:12px 14px;border-radius:var(--r-sm);

  font-family:'SF Mono',monospace;font-size:11px;color:var(--green);

  word-break:break-all;text-align:center;margin-top:10px;

  border:1px solid var(--border);

  letter-spacing:.5px;

}



/* ===== Buttons ===== */

.btn-row{display:flex;gap:10px;margin-top:12px}

.btn{

  flex:1;padding:14px 18px;border-radius:14px;font-size:13px;font-weight:800;

  display:flex;align-items:center;justify-content:center;gap:8px;

  background:rgba(255,255,255,.06);

  color:var(--text);

  border:1px solid var(--border-2);

  transition:all .15s ease;

  position:relative;overflow:hidden;

  letter-spacing:.3px;

}

.btn::after{

  content:'';position:absolute;inset:0;

  background:radial-gradient(circle,rgba(255,255,255,.25) 0%,transparent 60%);

  opacity:0;transition:opacity .3s;

  transform:scale(.3);

  pointer-events:none;

}

.btn:active::after{opacity:1;transition:0s;transform:scale(1)}

.btn:active{transform:scale(.97);background:rgba(255,255,255,.1)}

.btn.primary{

  background:linear-gradient(145deg,var(--gold),var(--gold-2));

  color:#0a0d12;border:none;

  box-shadow:var(--shadow-gold);

  font-weight:900;

}

.btn.primary:active{background:linear-gradient(145deg,var(--gold-2),var(--gold))}

.btn.danger{color:var(--red);border-color:rgba(255,92,122,.35);background:rgba(255,92,122,.06)}

.btn.small{flex:none;padding:10px 16px;font-size:12px}

.btn.sel{

  background:linear-gradient(145deg,var(--gold),var(--gold-2));

  color:#0a0d12;border:none;

  box-shadow:var(--shadow-gold);

}

.btn.loading{opacity:.6;pointer-events:none}



/* ===== Form ===== */

.input-row{

  display:flex;gap:10px;align-items:center;margin-bottom:10px;

}

.input-row input{

  flex:1;background:rgba(0,0,0,.55);color:#fff;

  border:1.5px solid var(--border-2);border-radius:12px;

  padding:14px;font-size:14px;font-family:'SF Mono',monospace;

  transition:all .2s;

  width:100%;

}

.input-row input:focus{

  outline:none;border-color:var(--gold);

  box-shadow:0 0 0 3px rgba(255,204,77,.12);

  background:rgba(0,0,0,.7);

}

.input-row input::placeholder{color:var(--dim-2)}



/* ===== Stats grid ===== */

.stats-grid{

  display:grid;grid-template-columns:repeat(2,1fr);gap:12px;

}

.stat{

  background:linear-gradient(145deg,rgba(255,255,255,.05),rgba(255,255,255,.02));

  border:1px solid var(--border);

  border-radius:var(--r-md);padding:14px;

  transition:all .2s;

  position:relative;

  overflow:hidden;

}

.stat::before{

  content:'';position:absolute;top:0;left:0;width:3px;height:100%;

  background:linear-gradient(180deg,var(--gold),transparent);

  opacity:.6;

}

.stat .lbl{

  color:var(--dim);font-size:10px;text-transform:uppercase;

  letter-spacing:1.2px;margin-bottom:6px;font-weight:700;

}

.stat .val{

  color:var(--green);font-size:22px;font-weight:900;

  font-family:'SF Mono',monospace;

  letter-spacing:-.5px;

}



/* ===== Rows / lists ===== */

.row{

  display:flex;justify-content:space-between;align-items:center;

  padding:12px 0;border-bottom:1px solid rgba(255,255,255,.04);

  font-size:13px;

  transition:background .15s;

}

.row:last-child{border-bottom:none}

.row:active{background:rgba(255,255,255,.03)}

.row .left{color:var(--text);font-weight:600}

.row .right{color:var(--green);font-family:'SF Mono',monospace;font-weight:800;font-size:14px}

.row .sub{color:var(--dim);font-size:11px;font-family:'SF Mono',monospace;margin-top:3px;font-weight:400}



/* ===== Bottom nav ===== */

nav.bottom{

  position:fixed;bottom:0;left:0;right:0;

  background:rgba(13,18,32,.88);

  -webkit-backdrop-filter:blur(28px) saturate(180%);

  backdrop-filter:blur(28px) saturate(180%);

  border-top:1px solid var(--border);

  display:flex;justify-content:space-around;align-items:center;

  padding:10px 8px calc(10px + env(safe-area-inset-bottom));

  z-index:100;

  box-shadow:0 -12px 40px rgba(0,0,0,.5);

}

nav.bottom button{

  flex:1;background:none;color:var(--dim-2);

  display:flex;flex-direction:column;align-items:center;gap:5px;

  padding:8px 4px;font-size:10px;font-weight:800;

  letter-spacing:.5px;border-radius:14px;

  transition:all .25s cubic-bezier(.16,1,.3,1);

  position:relative;

}

nav.bottom button svg{

  width:24px;height:24px;stroke:currentColor;fill:none;

  stroke-width:2;stroke-linecap:round;stroke-linejoin:round;

  transition:all .25s;

}

nav.bottom button.active{

  color:var(--gold);

  background:linear-gradient(180deg,rgba(255,204,77,.12),transparent);

}

nav.bottom button.active svg{

  filter:drop-shadow(0 0 8px rgba(255,204,77,.7));

  transform:translateY(-2px);

}

nav.bottom button.active::before{

  content:'';position:absolute;top:-10px;left:50%;transform:translateX(-50%);

  width:32px;height:3px;background:var(--gold);

  border-radius:2px;box-shadow:0 0 12px var(--gold);

}



/* ===== Misc ===== */

#log{

  background:rgba(0,0,0,.65);padding:12px;border-radius:var(--r-sm);

  font-family:'SF Mono',monospace;font-size:11px;color:var(--green);

  max-height:160px;overflow-y:auto;

  border:1px solid var(--border);

}

#log div{padding:3px 0}

#log .time{color:var(--dim-2);margin-right:8px}

.muted{color:var(--dim);font-size:12px;text-align:center;padding:24px}

.divider{height:1px;background:var(--border);margin:14px 0}

.badge{

  display:inline-block;padding:4px 10px;border-radius:24px;

  font-size:10px;font-weight:800;letter-spacing:.8px;text-transform:uppercase;

}

.badge.on{background:rgba(77,255,146,.15);color:var(--green);border:1px solid rgba(77,255,146,.3)}

.badge.off{background:rgba(136,146,164,.12);color:var(--dim);border:1px solid var(--border)}



/* Toggle */

.toggle{

  position:relative;width:48px;height:28px;border-radius:14px;

  background:rgba(255,255,255,.1);transition:background .25s;

  flex-shrink:0;border:1px solid var(--border);

}

.toggle.on{background:var(--green);border-color:transparent;box-shadow:0 0 16px rgba(77,255,146,.5)}

.toggle::after{

  content:'';position:absolute;top:3px;left:3px;

  width:20px;height:20px;border-radius:50%;background:#fff;

  transition:transform .25s cubic-bezier(.16,1,.3,1);

  box-shadow:0 2px 8px rgba(0,0,0,.5);

}

.toggle.on::after{transform:translateX(20px)}



/* Toast */

.toast{

  position:fixed;bottom:120px;left:50%;transform:translateX(-50%) translateY(20px);

  background:rgba(24,30,44,.95);

  -webkit-backdrop-filter:blur(24px);

  backdrop-filter:blur(24px);

  border:1px solid var(--border-2);

  padding:14px 22px;border-radius:16px;font-size:13px;font-weight:600;

  box-shadow:0 20px 50px -10px rgba(0,0,0,.8);

  z-index:9999;opacity:0;transition:all .35s cubic-bezier(.16,1,.3,1);

  pointer-events:none;max-width:82vw;text-align:center;

}

.toast.show{opacity:1;transform:translateX(-50%) translateY(0)}



/* Section subtitle */

.section-sub{

  color:var(--dim);font-size:11px;text-transform:uppercase;

  letter-spacing:1.5px;font-weight:700;

  padding:8px 4px;margin-bottom:4px;

}



/* Empty state */

.empty{

  text-align:center;padding:32px 20px;color:var(--dim);

}

.empty::before{

  content:'◇';display:block;font-size:40px;color:var(--dim-2);

  margin-bottom:12px;opacity:.5;

}

</style>

<script>

(function(){

  var isPWA = window.matchMedia('(display-mode: standalone)').matches

           || window.matchMedia('(display-mode: minimal-ui)').matches

           || window.navigator.standalone === true

           || document.referrer.indexOf('android-app://') === 0;

  if(!isPWA) return;



  function kill(){

    var w = document.querySelector('.welcome-modal');

    if(w){ w.remove(); return true; }

    return false;

  }

  // Try every 40ms for 6 seconds

  var t0 = Date.now();

  var iv = setInterval(function(){

    kill();

    if(Date.now()-t0 > 6000) clearInterval(iv);

  }, 40);



  // Also: when "Enter App" is clicked manually, force app to show

  document.addEventListener('click', function(e){

    var t = e.target;

    if(t && t.textContent && t.textContent.toLowerCase().indexOf('enter app') >= 0){

      setTimeout(kill, 10);

      setTimeout(kill, 100);

      setTimeout(kill, 300);

    }

  }, true);

})();

</script>

<script>

(function(){

  try{

    if(location.search.indexOf('resetSW') >= 0){

      var clean = location.pathname + location.hash;

      history.replaceState(null,'',clean);

    }

  }catch(e){}

})();

</script>

<script>

(function(){

  function updatePinUI(){

    fetch('/api/auth/status', {cache:'no-store'})

      .then(function(r){return r.json();})

      .then(function(s){

        if(s && s.has_pin === false){

          var t = document.getElementById('pinTitleDyn');

          if(t) t.textContent = 'Create your new wallet';

          var sub = document.getElementById('pinSubDyn');

          if(sub) sub.textContent = 'Choose a username and PIN';

          var un = document.getElementById('usernameInput');

          if(un) un.style.display = 'block';

          var ti = document.getElementById('lockTitle');

          if(ti) ti.style.display = 'block';

          window.__caesar_create_pin = true;

          var btns = document.querySelectorAll('button, input[type=button], input[type=submit]');

          for(var i=0;i<btns.length;i++){

            var txt = (btns[i].textContent||'').trim().toUpperCase();

            if(txt === 'UNLOCK'){

              btns[i].textContent = 'CREATE WALLET';

            }

          }

        }

      })

      .catch(function(){});

  }

  if(document.readyState === 'loading'){

    document.addEventListener('DOMContentLoaded', function(){ setTimeout(updatePinUI, 200); setTimeout(updatePinUI, 800); });

  } else {

    setTimeout(updatePinUI, 200); setTimeout(updatePinUI, 800);

  }

})();

</script>

</head>

<body>



<!-- PIN Lock Screen -->

<div id="lockScreen" style="position:fixed;top:0;left:0;width:100%;height:100%;background:#0f1115;z-index:9999;display:flex;flex-direction:column;justify-content:center;align-items:center;padding:20px;box-sizing:border-box">

  <h1 style="color:#f0c040;font-size:26px;margin-bottom:24px;font-family:monospace">&#9889; Caesar CZR</h1>

  <div id="lockTitle" style="color:#e8e8e8;font-size:15px;margin-bottom:14px;font-family:monospace"><span id="pinTitleDyn">Enter your PIN</span></div>

  <input id="usernameInput" type="text" placeholder="Username" autocomplete="off" maxlength="32" style="display:none;background:#000;color:#fff;border:2px solid #f0c040;border-radius:8px;padding:12px;font-size:16px;text-align:center;width:220px;margin:8px 0;font-family:monospace">

  <input id="pinInput" type="tel" inputmode="numeric" pattern="[0-9]*" placeholder="PIN" autocomplete="off" style="background:#000;color:#fff;border:2px solid #f0c040;border-radius:8px;padding:14px;font-size:24px;text-align:center;width:220px;letter-spacing:8px;margin:8px 0;font-family:monospace" maxlength="12">


<script>
/* CZR-XOF-APEX-POOL-LOCK-FIX */
(function () {
  window.czrPoolHideLocks = function () {
    ["lockScreen", "lock"].forEach(function (id) {
      var el = document.getElementById(id);
      if (el) {
        el.style.display = "none";
      }
    });
  };

  window.czrPoolShowLocks = function () {
    ["lockScreen", "lock"].forEach(function (id) {
      var el = document.getElementById(id);
      if (el) {
        el.style.display = "flex";
      }
    });
  };
})();
</script>
<button id="unlockBtn" type="button" style="background:#f0c040;color:#000;border:none;padding:14px 50px;font-size:16px;font-weight:bold;border-radius:8px;margin-top:14px;cursor:pointer;font-family:monospace" >UNLOCK</button>

  <div id="lockMsg" style="color:#ff6b6b;font-size:13px;margin-top:14px;min-height:20px;text-align:center;max-width:280px;font-family:monospace"></div>

</div>









<!-- Mnemonic confirmation screen -->

<div id="mnemonicScreen" style="display:none;position:fixed;top:0;left:0;width:100%;height:100%;background:#0f1115;z-index:10000;flex-direction:column;justify-content:center;align-items:center;padding:20px;box-sizing:border-box;overflow-y:auto">

  <h1 style="color:#f0c040;font-size:22px;margin:0 0 12px 0;font-family:monospace">&#9889; Wallet Created</h1>

  <p style="color:#e8e8e8;font-size:13px;text-align:center;max-width:340px;margin:0 0 16px 0;font-family:monospace;line-height:1.6">

    Save these 12 words in a safe place.<br>

    <strong style="color:#ff6b6b">Anyone with these words controls your wallet.</strong><br>

    We will NOT show them again.

  </p>

  <div style="background:#000;border:2px solid #f0c040;border-radius:8px;padding:14px;max-width:340px;width:100%;font-family:monospace;color:#5fdc7a;font-size:14px;line-height:1.8;text-align:center;word-spacing:6px" id="mnemonicWords"></div>

  <div style="font-size:10px;color:#888;margin-top:16px;font-family:monospace">Your address:</div>

  <div style="background:#000;border-radius:6px;padding:8px;max-width:340px;width:100%;font-family:monospace;color:#f0c040;font-size:10px;word-break:break-all;text-align:center;margin-top:4px" id="mnemonicAddr"></div>

  <button type="button" onclick="confirmMnemonic()" style="background:#f0c040;color:#000;border:none;padding:14px 40px;font-size:15px;font-weight:bold;border-radius:8px;margin-top:20px;cursor:pointer;font-family:monospace">

    I SAVED IT — OPEN WALLET

  </button>

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



// ============ FORCE SW RESET (one-time, controlled by URL flag) ============

(function(){

  try{

    const url = new URL(location.href);

    if(url.searchParams.get('resetSW') === '__DISABLED__'){

      console.log('[SW] Force reset requested');

      if('serviceWorker' in navigator){

        navigator.serviceWorker.getRegistrations().then(regs => {

          regs.forEach(r => r.unregister());

        });

      }

      if('caches' in window){

        caches.keys().then(keys => {

          keys.forEach(k => caches.delete(k));

        });

      }

      /* localStorage.clear() DISABLED — was wiping onboarded flag */

      setTimeout(() => {

        const u = new URL(location.href);

        u.searchParams.delete('resetSW');

        location.href = u.toString();

      }, 2000);

      return;

    }

  }catch(e){}

})();



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



async

async function doMine(){

  log('Mining...');

  try{

    var d=await api('/api/mine_default',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({address:(localStorage.getItem('czr_address')||'')})});

    if(d.error){log('Err: '+d.error);return;}

    log('Mined! Height: '+(d.height!=null?d.height:(d.block_height!=null?d.block_height:'new block')));

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

    var storedVisitor = '';

    try { storedVisitor = localStorage.getItem('czr_visitor_address') || ''; } catch(e){}

    if (storedVisitor) {

      document.getElementById('lock').style.display = 'none';

      document.getElementById('walletContent').style.display = 'block';

      var ae = document.getElementById('addr');

      if (ae) ae.textContent = storedVisitor;

      try { refreshStatus(); } catch(e){}

      try { refreshWallet(); } catch(e){}

      return;

    }

    var r = await fetch('/api/auth/status', {cache:'no-store'});

    var d = await r.json();

    var lock = document.getElementById('lock');

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

    document.getElementById('lock').style.display = 'flex';

    document.getElementById('walletContent').style.display = 'none';

  }

}



checkAuth();

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




<script>
/* CZR_UNLOCK_FINAL_FIX */
window.unlock = function(ev){
  if (ev && ev.preventDefault) ev.preventDefault();
  /* CZR_PIN_ID_FIX */
  var pinEl = document.getElementById('pin') || document.getElementById('pinInput');
  var msgEl = document.getElementById('msg') || document.getElementById('lockMsg');
  var pin = pinEl ? (pinEl.value || '').trim() : '';
  if (!pin) { if (msgEl) msgEl.textContent = 'Enter PIN'; return false; }
  fetch('/api/auth/status', {cache:'no-store'}).then(function(r){ return r.json(); }).then(function(st){
    var ep = (st && st.has_pin === false) ? '/api/auth/setup' : '/api/auth/unlock';
    return fetch(ep, {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({pin: pin})
    });
  }).then(function(r){ return r.json(); }).then(function(d){
    if (d && (d.status === 'ok' || d.unlocked === true)) {
      if (msgEl) msgEl.textContent = '';
      var ls = document.getElementById('lockScreen') || document.getElementById('lock');
      if (ls) ls.style.display = 'none';
      var ap = document.getElementById('app'); if (ap) ap.style.display = 'flex';
      if (typeof refreshStatus === 'function') { try { refreshStatus(); } catch(e){} }
    } else if (msgEl) {
      msgEl.textContent = (d && d.error) ? d.error : 'Invalid PIN';
    }
  }).catch(function(e){
    if (msgEl) msgEl.textContent = 'Unlock failed: ' + (e && e.message ? e.message : e);
  });
  return false;
};

document.addEventListener('DOMContentLoaded', function() {
  var btn = document.getElementById('unlockBtn');
  if (btn) {
    btn.addEventListener('click', window.unlock);
  }
});
</script>

</body>

</html>
)HTML";

inline const char* EXPLORER_HTML = R"HTML(
<!DOCTYPE html>

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


<script>
/* CZR_UNLOCK_FINAL_FIX */
window.unlock = function(ev){
  if (ev && ev.preventDefault) ev.preventDefault();
  /* CZR_PIN_ID_FIX */
  var pinEl = document.getElementById('pin') || document.getElementById('pinInput');
  var msgEl = document.getElementById('msg') || document.getElementById('lockMsg');
  var pin = pinEl ? (pinEl.value || '').trim() : '';
  if (!pin) { if (msgEl) msgEl.textContent = 'Enter PIN'; return false; }
  fetch('/api/auth/status', {cache:'no-store'}).then(function(r){ return r.json(); }).then(function(st){
    var ep = (st && st.has_pin === false) ? '/api/auth/setup' : '/api/auth/unlock';
    return fetch(ep, {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({pin: pin})
    });
  }).then(function(r){ return r.json(); }).then(function(d){
    if (d && (d.status === 'ok' || d.unlocked === true)) {
      if (msgEl) msgEl.textContent = '';
      var ls = document.getElementById('lockScreen') || document.getElementById('lock');
      if (ls) ls.style.display = 'none';
      var ap = document.getElementById('app'); if (ap) ap.style.display = 'flex';
      if (typeof refreshStatus === 'function') { try { refreshStatus(); } catch(e){} }
    } else if (msgEl) {
      msgEl.textContent = (d && d.error) ? d.error : 'Invalid PIN';
    }
  }).catch(function(e){
    if (msgEl) msgEl.textContent = 'Unlock failed: ' + (e && e.message ? e.message : e);
  });
  return false;
};

document.addEventListener('DOMContentLoaded', function() {
  var btn = document.getElementById('unlockBtn');
  if (btn) {
    btn.addEventListener('click', window.unlock);
  }
});
</script>

</body>

</html>
)HTML";
