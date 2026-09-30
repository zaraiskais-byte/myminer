# Changelog

## [Unreleased]

### Added
- include/caesar/bip39.hpp - BIP39 mnemonic encode/decode with
  PBKDF2-HMAC-SHA512 seed derivation (Trezor vectors pass).
- include/caesar/bip39_wordlist.hpp - 2048-word English wordlist.
- include/caesar/slip10.hpp - SLIP-0010 Ed25519 hierarchical
  derivation (spec vectors pass).
- include/caesar/wallet_hd.hpp - deterministic wallet from mnemonic.
- tests/bip39_test.cpp, tests/slip10_test.cpp, tests/wallet_hd_test.cpp.
- POST /api/auth/recover - restore wallet from BIP39 mnemonic.
- POST /api/auth/encrypt-wallet - encrypt existing plaintext
  wallet.pem without changing its address.
- PersistentWallet::save_encrypted() - atomic AES-256-GCM write.
- PersistentWallet::unlock_with_pin() - decrypt and parse on demand.
- PersistentWallet::load_or_defer() - defer load when encrypted.
- PersistentWallet::is_file_encrypted(), is_loaded().
- SECURITY.md - threat model and cryptographic parameters.

### Changed
- load_or_create() replaced by load_or_defer(). Encrypted files
  no longer cause startup failure.
- /api/auth/setup returns mnemonic and address for fresh wallets.
- /api/auth/unlock decrypts wallet.pem if it is encrypted.

### Removed
- /api/auth/seed endpoint (returned a fake recovery phrase).
- generate_seed_phrase() and the truncated 256-word table.

### Fixed
- Atomic write ordering in PersistentWallet::save().
- Refuses to silently regenerate the wallet when an existing file
  is unreadable or encrypted.
