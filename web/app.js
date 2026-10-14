'use strict';

function $(id) {
    return document.getElementById(id);
}

function toast(msg) {
    let t = $('toast');
    if (!t) {
        t = document.createElement('div');
        t.id = 'toast';
        t.style.cssText = 'display:none;position:fixed;bottom:20px;left:50%;transform:translateX(-50%);background:#238636;color:#fff;padding:10px 20px;border-radius:6px;z-index:9999;font-weight:bold;';
        document.body.appendChild(t);
    }
    t.textContent = msg;
    t.style.display = 'block';
    setTimeout(() => { t.style.display = 'none'; }, 2500);
}

async function unlock(event) {
    if (event) event.preventDefault();

    const pinInput = $('pin') || $('pinInput');
    const msg = $('msg') || $('lockMsg');
    const button = $('unlockBtn');
    const pin = pinInput ? pinInput.value.trim() : '';

    if (!pin || pin.length < 4) {
        if (msg) msg.textContent = 'Please enter PIN (4+ digits)';
        return false;
    }

    if (msg) msg.textContent = 'Unlocking...';
    if (button) button.disabled = true;

    try {
        const response = await fetch('/api/auth/unlock?_t=' + Date.now(), {
            method: 'POST',
            credentials: 'same-origin',
            cache: 'no-store',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ pin })
        });

        let data;
        try {
            data = await response.json();
        } catch (_) {
            throw new Error('Invalid server response');
        }

        if (!response.ok || !data || data.unlocked !== true) {
            throw new Error(data && data.error ? data.error : 'Wallet unlock failed');
        }

        document.querySelectorAll('#lock, #lockScreen').forEach(el => {
            el.style.display = 'none';
        });
        document.querySelectorAll('#app').forEach(el => {
            el.style.display = 'flex';
        });

        if (msg) msg.textContent = '';
        toast('Wallet Unlocked ✓');

        if (typeof window.refreshStatus === 'function') {
            try { await window.refreshStatus(); } catch (_) {}
        }
        if (typeof window.refreshAll === 'function') {
            try { await window.refreshAll(); } catch (_) {}
        }
        return true;
    } catch (error) {
        if (msg) msg.textContent =
            error && error.message ? error.message : 'Wallet unlock failed';
        return false;
    } finally {
        if (button) button.disabled = false;
    }
}

window.unlock = unlock;

window.addEventListener('load', () => {
    const button = $('unlockBtn');
    if (button) button.onclick = unlock;
});
