# CARR Safety Policy

## Autonomous actions

The following classes may become autonomous after verification:

- VERIFY_CHAIN
- QUARANTINE_PEER
- ABORT_SYNC
- RECONNECT_PEER
- RESTART_SYNC
- REBUILD_UTXO
- RECOVER_STORAGE
- RESTART_P2P
- RESTART_NODE
- CREATE_REPLAY_CASE

## Consensus boundary

`CONSENSUS_CHANGE_REQUIRES_GATE` is never autonomous.

A C++ bug in consensus code may be diagnosed and reproduced automatically, but a consensus-rule modification requires a separate controlled validation path.

## Principle

Recovery may restore an existing verified protocol state.

Recovery must not invent a new protocol rule.
