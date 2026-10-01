# Security Model - Caesar CZR Wallet

## Layers

1. BIP39 for deterministic key generation and recovery.
2. SLIP-0010 for Ed25519 hierarchical key derivation.
3. AES-256-GCM for encrypted at-rest storage.
4. PBKDF2-HMAC-SHA256 (100k iterations) for PIN-derived keys.
5. Atomic write plus fsync to prevent corruption.

## Key Generation

    entropy (128 bits)
      -> mnemonic (12 words, BIP39 English)
      -> seed (64 bytes, PBKDF2-HMAC-SHA512, 2048 iter)
      -> SLIP-0010 m/44'/0'/0'/0'/0' (hardened only)
      -> 32-byte Ed25519 seed
      -> EVP_PKEY (Ed25519)

The mnemonic is the sole source of truth for recovery.

## Storage Format

- wallet.pem:
  - Legacy: PEM Ed25519 private key (119 bytes typical).
  - Encrypted: salt(16) || iv(12) || tag(16) || ciphertext.
- pin.hash: salt(16) || pbkdf2_hash_hex(64 bytes ASCII).
- session.txt: transient session token.
- blockchain.dat: public chain state.

## Lifecycle

1. First run: load_or_defer() creates a random wallet.
2. Encrypted file: load_or_defer() detects the AES envelope
   (first byte not '-') and defers loading.
3. Unlock: verify_pin first, then unlock_with_pin decrypts and
   parses the PEM. Any failure aborts the unlock.
4. Migration: POST /api/auth/encrypt-wallet encrypts an existing
   plaintext wallet without changing its identity.

## Derivation Path and Coin Type

CZR wallets use the derivation path:

    m / 44' / 7999' / 0' / 0' / 0'

- 44' is the purpose level (BIP-44).
- 7999' is CZR's coin type. This number is NOT registered in the
  official SLIP-44 registry. It is in the 7000-7999 range which the
  SLIP-44 maintainers leave unallocated for developer and test use.
  If CZR ever becomes a production cryptocurrency, a formal
  registration request should be submitted to the SLIP-44 registry
  and this constant updated. That would be a hard fork for existing
  wallets.
- All levels are hardened because CZR uses Ed25519 (via SLIP-0010),
  which forbids non-hardened derivation.

Same mnemonic on CZR produces a different keypair than on Bitcoin
or any other chain, because the coin type differs. This is
intentional and standard.

## Threat Model

Protected against:
- Lost device with encrypted wallet.
- Partial writes (atomic rename + fsync).
- Mnemonic loss (deterministic recovery).
- Impersonation (Ed25519 signatures).
- Silent regeneration (fail-closed on unreadable files).

Not protected against:
- Rooted device with memory access.
- Keylogger or compromised browser.
- Weak PIN subject to offline brute force.
- Theft of both wallet.pem and pin.hash plus compute.

## Recommendations

- PIN of at least 8 digits.
- Offline copy of the 12-word mnemonic.
- Do not expose port 8443 to the internet.
- Run only on trusted devices.

## Cryptographic Primitives

| Purpose            | Algorithm              | Parameters             |
|--------------------|------------------------|------------------------|
| Mnemonic encoding  | BIP39 English wordlist | 128-bit entropy        |
| Seed derivation    | PBKDF2-HMAC-SHA512     | 2048 iterations        |
| HD derivation      | SLIP-0010 Ed25519      | m/44'/7999'/0'/0'/0'   |
| PIN key derivation | PBKDF2-HMAC-SHA256     | 100k iterations        |
| Storage encryption | AES-256-GCM            | 96-bit IV, 128-bit tag |
