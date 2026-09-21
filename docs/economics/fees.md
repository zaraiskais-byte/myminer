# Caesar Transaction Fee Policy v1

Status: DRAFT

## Purpose

This document defines the initial technical model for transaction fees in
Caesar/CZR.

The fee policy is protocol-visible and deterministic.

## Basis points

The protocol uses basis points.

- 10000 bps = 100%
- 10 bps = 0.10%

The current draft founder protocol fee is 10 bps.

This value is provisional and must not be treated as a final economic or
legal decision until protocol review is complete.

## Founder protocol fee

For an eligible transaction amount:

founder_fee = floor(transaction_amount * founder_fee_bps / 10000)

The calculation uses integer arithmetic.

No floating-point arithmetic is permitted in consensus fee calculation.

## Treasury

The founder protocol fee is associated with a fixed protocol treasury
identifier.

The treasury is separate from the founder's personal wallet.

The final mainnet treasury address must be explicitly documented and
committed as part of the network policy before activation.

## Supply

The founder protocol fee does not create new CZR.

It is part of the value transferred by the transaction.

The 21,000,000 CZR maximum supply is unchanged by this policy.

## Consensus

A future consensus integration must independently recompute the expected
founder fee from transaction data.

Nodes must not trust a fee value supplied by a miner, wallet, or remote peer.

## Changes

Changing the founder fee rate or treasury after mainnet activation is a
protocol change.

There must be no hidden runtime mechanism for changing these values.

## Scope

This document describes protocol fees only.

Exchange services, custody, managed services, payment services, and other
commercial activities may have separate fees and separate legal
requirements.
