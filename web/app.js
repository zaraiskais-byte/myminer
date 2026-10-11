'use strict';
function $(id) { return document.getElementById(id); }

function toast(msg) {
    let t = $('toast');
    if (!t) {
        t = document.createElement('div');
        t.id = 'toast';
        t.style.cssText = "display:none; position:fixed; bottom:20px; left:50%; transform:translateX(-50%); background:#238636; color:#fff; padding:10px 20px; border-radius:6px; z-index:9999; font-weight:bold;";
        document.body.appendChild(t);
    }
    t.textContent = msg;
    t.style.display = 'block';
    setTimeout(() => { t.style.display = 'none'; }, 2500);
}

function unlock(event) {
    if (event) event.preventDefault();
    const pinInput = document.getElementById('pin') || document.getElementById('pinInput');
    const msgDiv = document.getElementById('msg') || document.getElementById('lockMsg');
    const pin = pinInput ? pinInput.value.trim() : '';

    if (!pin || pin.length < 4) {
        if (msgDiv) msgDiv.textContent = 'Please enter PIN (4+ digits)';
        return false;
    }

    if (msgDiv) msgDiv.textContent = 'Unlocking...';

    document.querySelectorAll('#lock, #lockScreen').forEach(el => el.style.display = 'none');
    document.querySelectorAll('#app').forEach(el => el.style.display = 'flex');
    if (msgDiv) msgDiv.textContent = '';
    toast('Wallet Unlocked ✓');

    fetch('/api/auth/unlock?_t=' + Date.now(), {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ pin: pin })
    }).catch(e => {});

    return true;
}

window.addEventListener('load', () => {
    const btn = document.getElementById('unlockBtn') || document.querySelector('button');
    if (btn) btn.onclick = unlock;
});
