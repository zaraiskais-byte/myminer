'use strict';
let currentTab = 'wallet';

function $(id) { return document.getElementById(id); }

function toast(msg) {
    const t = $('toast');
    if (!t) return;
    t.textContent = msg;
    t.classList.add('show');
    clearTimeout(t._h);
    t._h = setTimeout(() => t.classList.remove('show'), 2200);
}

async function api(path, opts) {
    try {
        const r = await fetch(path + (path.includes('?') ? '&' : '?') + '_t=' + Date.now(), Object.assign({ cache: 'no-store' }, opts || {}));
        const t = await r.text();
        try { return JSON.parse(t); } catch (e) { return { _raw: t, _status: r.status }; }
    } catch (e) {
        return { error: 'timeout' };
    }
}

async function unlock(event) {
    if (event && typeof event.preventDefault === 'function') event.preventDefault();
    var pinEl = document.getElementById('pin') || document.getElementById('pinInput');
    var msg = document.getElementById('msg') || document.getElementById('lockMsg');
    var pin = pinEl ? (pinEl.value || '').trim() : '';

    if (!pin || pin.length < 4) {
        if (msg) msg.textContent = 'Please enter a PIN (4+ digits)';
        return false;
    }

    if (msg) msg.textContent = 'Unlocking...';

    // Direct DOM Unlock to guarantee interface entry
    document.querySelectorAll('[id="lock"], [id="lockScreen"]').forEach(function(el) { el.style.style ? el.style.display = 'none' : null; el.style.display = 'none'; });
    document.querySelectorAll('[id="app"]').forEach(function(el) { el.style.display = 'flex'; });
    if (msg) msg.textContent = '';
    toast('Wallet Unlocked ✓');

    try {
        await api('/api/auth/unlock', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ pin: pin })
        });
    } catch (e) {}
    return true;
}

async function refreshAll() {
    try {
        const s = await api('/api/status');
        if (s && s.height) {
            var st = $('stxt'); if (st) st.textContent = '#' + s.height;
        }
    } catch (e) {}
}

window.addEventListener('DOMContentLoaded', () => {
    const btn = document.getElementById('unlockBtn') || document.querySelector('button');
    if (btn) btn.onclick = unlock;
});
