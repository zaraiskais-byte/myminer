
## 20261025-2216 fix: invite circle shows 0
- Children lists are rebuilt from the saved parent field in /api/user/tree, /api/user/me and /api/me/info (the state loader read field 5 as both username and children).

## 20261025-2235 fix: hero counters
- /api/pool/stats now returns blocks_today (chain blocks stamped today, UTC) and shares_today (since the daily baseline saved in daily_counters.txt); the page shows them instead of a fixed 0 and the all-time share total.
