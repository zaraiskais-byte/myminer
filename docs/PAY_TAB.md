# Pay tab
Fifth tab in the wallet. Paste a CZ1 address (or a link that contains one), optional account name, amount in CZR.
- Amount is converted to atomic units (1 CZR = 100000000) and sent to /api/send.
- The server still validates the address, balance and session (auth + csrf).
- Social account links do not contain wallet addresses. Platform balances (for example TikTok) cannot receive CZR.
- A local log of the last 50 payments is kept in the browser (localStorage key czr_pay_log).
