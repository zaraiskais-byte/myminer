'use strict';
function $(id) { return document.getElementById(id); }
function toast(msg, bad) {
  let t = $('toast');
  if (!t) {
    t = document.createElement('div'); t.id = 'toast';
    t.style.cssText = 'display:none;position:fixed;bottom:20px;left:50%;transform:translateX(-50%);color:#fff;padding:10px 20px;border-radius:6px;z-index:9999;font-weight:bold;';
    document.body.appendChild(t);
  }
  t.style.background = bad ? '#da3633' : '#238636';
  t.textContent = msg; t.style.display = 'block';
  setTimeout(() => { t.style.display = 'none'; }, 3500);
}
function findPin() {
  return $('pin') || $('pinInput') ||
    document.querySelector('input[type=password]') ||
    document.querySelector('input[inputmode=numeric]') ||
    document.querySelector('input[type=tel]') ||
    Array.from(document.querySelectorAll('input')).pop();
}
function findName(p) { return Array.from(document.querySelectorAll('input')).find(i => i !== p); }
let busy = false;
async function unlock(event) {
  if (event && event.preventDefault) event.preventDefault();
  if (busy) return false;
  const pinInput = findPin(), nameInput = findName(pinInput);
  const msg = $('msg') || $('lockMsg');
  const button = $('unlockBtn') || Array.from(document.querySelectorAll('button')).find(b => /unlock/i.test(b.textContent));
  const say = (t, bad) => { if (msg) msg.textContent = t; if (bad) toast(t, true); };
  const pin = pinInput ? pinInput.value.trim() : '';
  const name = nameInput ? nameInput.value.trim() : '';
  if (!pin || pin.length < 4) { say('Please enter PIN (4+ digits)', true); return false; }
  busy = true; say('Unlocking...'); if (button) button.disabled = true;
  try {
    const r = await fetch('/api/auth/unlock?_t=' + Date.now(), {
      method: 'POST', credentials: 'same-origin', cache: 'no-store',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ pin, name })
    });
    let d;
    try { d = await r.json(); } catch (_) { throw new Error('Invalid server response (HTTP ' + r.status + ')'); }
    if (!r.ok || !d || d.unlocked !== true) throw new Error(d && d.error ? d.error : 'Wallet unlock failed (HTTP ' + r.status + ')');
    document.querySelectorAll('#lock, #lockScreen').forEach(el => { el.style.display = 'none'; });
    document.querySelectorAll('#app').forEach(el => { el.style.display = 'flex'; });
    say(''); toast('Wallet Unlocked ✓');
    if (typeof window.refreshStatus === 'function') { try { await window.refreshStatus(); } catch (_) {} }
    if (typeof window.refreshAll === 'function') { try { await window.refreshAll(); } catch (_) {} }
    return true;
  } catch (e) {
    say(e && e.message ? e.message : 'Wallet unlock failed', true);
    return false;
  } finally { busy = false; if (button) button.disabled = false; }
}
window.unlock = unlock;
function init() {
  const b = $('unlockBtn') || Array.from(document.querySelectorAll('button')).find(x => /unlock/i.test(x.textContent));
  if (b) b.addEventListener('click', unlock);
  const p = findPin();
  if (p) p.addEventListener('keydown', e => { if (e.key === 'Enter') unlock(e); });
  document.querySelectorAll('form').forEach(f => f.addEventListener('submit', unlock));
}
if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', init); else init();
