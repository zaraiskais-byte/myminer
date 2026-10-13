# Caesar CZR -- CZR-XOF-APEX -- Project Charter

## Part 1 -- Real Integration

1. The project is a real integration of wallet node chain mining pool and interfaces.
2. No claim of success before real on device testing.
3. No modification to wallet chain or payment data before integration passes and tests are safe.
4. Every change follows the cycle inspect modify build test push.
5. The single source of truth is the official GitHub repository.

## Part 2 -- Royal Core and Self Management

6. A Royal Core kernel manages operational decisions.
7. Self build -- the program builds itself when needed.
8. Self run -- boots fully without human intervention.
9. Self development -- proposes improvements and pushes after verification.
10. Major maintenance -- at big changes with full backup.
11. Minor maintenance -- periodic daily or weekly log cleanup.
12. Self discovery of errors defects and conflicts.
13. Self repair with rollback on failure.
14. Self push after tests pass.
15. Self test before every push.
16. Self backup of chain wallet and keys.
17. Periodic reports to the original owner only.
18. Owner protection -- no automatic operation touches owner data.
19. Self governance with safe limits.

## Part 3 -- Required Features

20. Automatic login with verified email via Credential Manager.
21. OTP on first registration only.
22. Session persistence after verification so OTP is not requested again.
23. Full owner privileges over wallet node and pool.
24. Real working referral link that records referrals.
25. Mining worker bound to the actual wallet address.
26. Rewards and transfers with duplicate spend prevention.
27. Pool balances matched with chain and payment history.

## Part 4 -- Current Status

- Level 1 OTP store -- done and tested.
- Level 2 OTP server endpoints -- done and tested.
- Level 3 Android application -- built signed installed and opens the UI.
- SMTP -- infrastructure ready waiting for Gmail key for real sending.

## Part 5 -- Real Test Results

- OTP request generate verify -- passed on device.
- SMTP fallback log mode -- passed.
- Real ports -- 8443 for RPC and 18555 for P2P.
- Alternative ports for conflict avoidance -- 38543 and 39555.
- Pool fee -- 2 percent corrected from 35.
- Chain -- responds.
- Interface -- served with HTTP 200.
- Mining worker -- 134 shares at 71.7 hashes per second on device.
