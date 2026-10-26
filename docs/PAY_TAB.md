# Pay tab
Fifth tab in the wallet. Paste a CZ1 address (or a link that contains one), optional account name, amount in CZR.
- Amount is converted to atomic units (1 CZR = 100000000) and sent to /api/send.
- The server still validates the address, balance and session (auth + csrf).
- Social account links do not contain wallet addresses. Platform balances (for example TikTok) cannot receive CZR.
- A local log of the last 50 payments is kept in the browser (localStorage key czr_pay_log).

## v2 (queue)
- Field 1 name, field 2 CZ1 address or QR (camera or image), field 3 amount in CZR, then Review and Confirm.
- If the server answers insufficient balance, the payment waits in a local queue and is retried every 30 s while the page is open.
- Payments are saved before sending. Interrupted or unanswered sends become "check" and are never retried automatically.
- TikTok links and QR codes contain no wallet address; use the QR from the Receive card of the CZR wallet.

## v3 (four fields)
- 1 name, 2 profile link or QR (TikTok etc., record only, never a destination), 3 wallet address CZ1 (paste, scan or image), 4 amount in CZR.
- A scanned QR that holds a web link goes to field 2; a CZ1 address goes to field 3.
- The profile link is shown in the confirm box and in the payments list. Money only goes to the CZ1 address.

## v3.1
- The fifth tab is labelled TikTok with a music-note icon. Tab id and send logic are unchanged (data-tab="pay").
- Present only in the main wallet page; sending still needs the unlocked PIN session.

## v4 (confirmation code + send log)
- Confirm opens a 6-digit code (POST /api/send/prepare). The code is typed back, then /api/send checks it: 5 min, 3 attempts, bound to recipient and amount, consumed after a successful send.
- A request timeout is now shown as "check" (the transfer may have gone out), never "failed".
- Stage timings are appended to ~/czr-send.log (utxos_ms, submit_ms).

## v5 (send timeout + accept timing)
- The Pay page waits up to 120 s for /api/send (was 30 s), so a slow but successful send shows "sent".
- accept_transaction() appends lock_ms, load_ms, rebuild_ms, mempool_ms and relay_ms to ~/czr-send.log, to find where a send spends its time.

## v6 (UTXO cache for transaction admission)
- accept_transaction() keeps the UTXO set in memory, keyed by the chain tip hash.
- Same tip: reused as is. New blocks (up to 64): applied incrementally. Anything else: full rebuild as before.
- Block building and block validation still use rebuild_utxo_set(), so consensus is unchanged.
- ~/czr-send.log now shows path=hit, path=incr or path=full.
