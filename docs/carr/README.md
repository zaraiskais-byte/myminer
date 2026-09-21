# CARR

Caesar Autonomous Recovery and Repair.

CARR is the recovery and incident-control subsystem for Caesar/CZR.

## Scope

CARR operates on:

- caesard process failures
- P2P failures
- invalid peer data
- chain synchronization failures
- UTXO consistency failures
- blockchain storage failures
- deterministic replay cases
- verified recovery

CARR does not replace consensus.

## Safety boundary

Consensus rule changes are never autonomous.

A future consensus-changing proposal must pass an explicit gate outside the normal recovery path.

Operational recovery may be autonomous only when the policy engine explicitly permits the action.

## Incident lifecycle

DETECTED
-> CAPTURED
-> DIAGNOSED
-> PLANNED
-> VERIFYING
-> RECOVERED

Uncertain or unsafe incidents are escalated.

## Current implementation

The first CARR core contains:

- typed incident records
- deterministic diagnosis
- repair planning
- safety policy
- verification planning
- append-only JSON incident evidence
- automated unit/integration test coverage

The current implementation is deliberately independent from the existing Caesar consensus and P2P source files.

## Planned integration

The next integration layers are:

1. caesard runtime evidence
2. P2P peer quarantine
3. Chain Sync control
4. chain verification
5. UTXO rebuild
6. storage recovery
7. fault injection
8. replay generation
9. sanitizer and fuzz validation
10. telemetry export
11. signed build provenance
12. bounded candidate patch workflow

CARR must never silently change consensus rules or publish an unverified consensus implementation.
