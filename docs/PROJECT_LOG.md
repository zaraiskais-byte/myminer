# Session Log -- Caesar CZR

## 2026-10-13 and 2026-10-14

### Completed Commits

1. 1527c76 -- remove stray empty file main from repository root
2. a0d885e -- fix pool fee from 35 to 2 percent
3. 10e386d -- add OTP store with secure code generation and tests
4. db2adfa -- add OTP request and verify endpoints to HTTP server
5. 571aa24 -- align help text and Android RPC port with actual defaults 8443 and 18555
6. 6b130ae -- add real SMTP sender via curl with config file and OTP fallback
7. 9361da9 -- fix CI using android-actions setup and NDK 26.1 with retries
8. 325ad6a -- add release signing config with v1 v2 v3 schemes
9. 7d62b5f -- add JS error handlers to unlock flow for diagnostics
10. 52fbe78 -- persist release keystore in repo for stable APK updates
11. 18dc2c2 -- replace unlock with step by step diagnostic version
12. 10bac92 -- use uncommon ports 38543 and 39555 and clear webview cache
13. 47cb3ec -- add version endpoint and auto release publishing

### Real Tests Passed

- 72 of 72 unit tests passed.
- OTP end to end request generate verify.
- APK build on GitHub Actions in 1 minute 54 seconds.
- APK install on Huawei JKM-LX1.
- Application opens and shows PIN screen.
- Server runs in Termux on 9443 and 19555.
- Chain pool and interface respond.
- Mining worker 134 shares 71.7 hashes per second.

### Issues Fixed

- Pool fee conflict 35 percent vs 2 percent.
- Port conflict between help text and code 18444 and 8332 vs 18555 and 8443.
- APK build failure due to broken NDK.
- APK install rejection due to missing v1 signature.
- APK update rejection due to per build signing key.

### Pending

- Confirm unlock button works on device.
- Confirm blue diagnostic bar appears on device.
- Add auto update button in frontend.
- Activate SMTP with real Gmail key.
- Google Sign-In or Credential Manager.
