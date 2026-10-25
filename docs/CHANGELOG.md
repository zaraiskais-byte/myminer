
## 20261025-2216 fix: invite circle shows 0
- Children lists are rebuilt from the saved parent field in /api/user/tree, /api/user/me and /api/me/info (the state loader read field 5 as both username and children).

## 20261025-2235 fix: hero counters
- /api/pool/stats now returns blocks_today (chain blocks stamped today, UTC) and shares_today (since the daily baseline saved in daily_counters.txt); the page shows them instead of a fixed 0 and the all-time share total.

## 20261025-2310 feature: names in the invite circle
- /api/user/tree and /api/me/info now return the username of each circle member; the main page and /me list them with level (the main page also shows an online dot from /api/pool/stats). Names are rendered with textContent.
