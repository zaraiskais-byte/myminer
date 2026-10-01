# Mining

## Overview

Caesar CZR uses a memory-hard proof-of-work. Mining consists of finding
a nonce such that:

    leading_zero_bits(calculate_pow_hash(header, nonce)) >= difficulty

The header is serialized with the nonce field zeroed.

## Solo Mining

The node can mine directly via:

    curl -X POST http://127.0.0.1:8443/api/mine_default

This is exposed in the web UI as the **Mine** button.

## Pool Mining

### For Workers

1. Register:

       POST /api/pool/register
       {"address": "CZ1...", "worker": "my-rig"}

2. Fetch a job:

       GET /api/pool/job?address=CZ1...

   Response:

       {
         "ok": true,
         "job_id": 42,
         "header_hex": "010000...",
         "share_difficulty": 4,
         "block_difficulty": 12,
         "height": 26
       }

3. Search for a nonce locally:

       for nonce in 0..2^64:
           hash = calculate_pow_hash(header_hex, nonce)
           if leading_zero_bits(hash) >= share_difficulty:
               submit share

4. Submit:

       POST /api/pool/submit
       {"address": "CZ1...", "job_id": 42, "nonce": 12345}

   Response:

       {
         "ok": true,
         "credited": true,
         "is_block": false,
         "block_added": false
       }

### Share Difficulty

Shares use an easier difficulty than full blocks:

    share_difficulty = block_difficulty - 8

This gives workers frequent feedback while blocks remain rare.

### Payouts

- Each credited share increments `shares_pending` for that worker.
- The pool wallet receives the coinbase of every block.
- Payouts distribute the pool wallet balance to workers:
  - 98% split by shares_pending
  - 2% pool fee

Payouts can be triggered:

- Manually: `POST /api/pool/payout`
- Automatically: enable via `POST /api/pool/auto/config`

Auto-payout triggers when `current_height >= last_payout_height + N`.

## Reference Worker

The bundled worker:

    ./build-web/caesar_worker \
      --pool http://pool-host:port \
      --address CZ1... \
      --name rig-1 \
      --threads 4

It:

1. Registers with the pool.
2. Loops:
   - Fetch a job
   - Search nonces locally
   - Submit shares
   - If a job is stale, request a new one

## Difficulty Floor

The minimum difficulty is 8 leading zero bits. This prevents an idle
network from dropping to difficulty zero.
