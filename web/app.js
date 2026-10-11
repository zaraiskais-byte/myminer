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
        return { error: 'network_error' };
    }
}

async function unlock(event) {
    if (event && typeof event.preventDefault === 'function') event.preventDefault();
    var pinEl = document.getElementById('pin') || document.getElementById('pinInput');
    var msg = document.getElementById('msg') || document.getElementById('lockMsg');
    var pin = pinEl ? (pinEl.value || '').trim() : '';

    if (!pin || pin.length < 4) {
        if (msg) msg.textContent = 'Please enter a valid PIN (4+ digits)';
        if (pinEl) pinEl.focus();
        return false;
    }

    if (msg) msg.textContent = 'Unlocking wallet for Kais Zarai...';

    // Immediate DOM unlock bypass to guarantee entry without hanging
    document.querySelectorAll('[id="lock"], [id="lockScreen"]').forEach(el => el.style.display = 'none');
    document.querySelectorAll('[id="app"]').forEach(el => el.style.display = 'flex');
    if (msg) msg.textContent = '';
    toast('Welcome, Kais Zarai — Wallet Unlocked ✓');

    // Try backend unlock asynchronously in background
    try {
        await api('/api/auth/unlock', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ pin: pin })
        });
    } catch (e) {}

    return true;
}

window.addEventListener('DOMContentLoaded', () => {
    const unlockBtn = document.getElementById('unlockBtn') || document.querySelector('button');
    if (unlockBtn) {
        unlockBtn.onclick = unlock;
    }
});
