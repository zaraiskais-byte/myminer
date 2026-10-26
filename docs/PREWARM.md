# UTXO cache prewarm

At node start a background thread builds the UTXO cache used by
accept_transaction. The chain is read under the chain mutex, the heavy
rebuild runs outside the lock, and the result is installed only if the
cache is still empty. Later tip changes use the incremental path.

Effect: the first payment after a restart no longer pays the full rebuild
(about 47 s on the phone). Progress is logged to ~/czr-send.log as
"[prewarm] ready ms=N". If the thread fails, accept_transaction falls back
to the old lazy path, so behavior stays correct.
