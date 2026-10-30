# Caesar (CZR) - technical listing sheet
Generated 20261025-1502 from source branch feature/me-wallet-forge, commit adfe709.

## Network facts (extracted from source code)
- Name: Caesar
- Ticker used in code: CZR
- Max supply: constexpr std::uint64_t CZR_MAX_SUPPLY = 21000000ULL * CZR_ATOMIC_UNITS;
- Atomic unit definition: constexpr std::uint64_t CZR_ATOMIC_UNITS = 100000000ULL;
- Decimals (derived from atomic unit): 8
- Target block time (seconds): 120
- Chain height when generated: 735
- Full list of consensus constants: docs/CONSTANTS.txt
- Source files mentioning signature schemes: docs/CRYPTO_FILES.txt
- Source files mentioning hash functions: docs/HASH_FILES.txt

## Public API
Read-only public routes: docs/PUBLIC_ROUTES.txt
Status route example: GET /api/status returns height, peers, mempool, uptime.
Blocks route: GET /api/blocks returns recent blocks (height, hash, prev, timestamp, difficulty, nonce, txs).
Block explorer page: /explorer

## Release integrity
Every node binary release is tagged in git (release-*), with its sha256 in docs/RELEASE-*.md.

## To complete before applying to any exchange or listing site (owner only)
- Official website URL and public node/explorer domain
- Official contact email and team/entity information
- Independent security audit report of the node and wallet
- Deposit confirmation policy and wallet integration notes
- Legal and compliance documents required by each platform
- Logo as PNG (square, 200x200 or larger)
