#include "carr_repair.hpp"

#include "carr_policy.hpp"

namespace caesar::carr {
namespace {

RepairCandidate candidate(
    ActionType action,
    const char* rationale,
    int risk,
    const Diagnosis& diagnosis) {

    RepairCandidate result;
    result.action = action;
    result.rationale = rationale;
    result.risk = risk;
    result.autonomous =
        policy_allows(diagnosis, action);

    return result;
}

}

std::vector<RepairCandidate>
plan_repairs(const Diagnosis& diagnosis) {

    std::vector<RepairCandidate> result;

    switch (diagnosis.type) {
    case IncidentType::INVALID_PEER_DATA:
        result.push_back(candidate(
            ActionType::QUARANTINE_PEER,
            "isolate the peer that supplied invalid chain data",
            10,
            diagnosis));

        result.push_back(candidate(
            ActionType::ABORT_SYNC,
            "stop the current synchronization attempt",
            15,
            diagnosis));

        result.push_back(candidate(
            ActionType::VERIFY_CHAIN,
            "verify local chain before recovery",
            5,
            diagnosis));

        result.push_back(candidate(
            ActionType::RESTART_SYNC,
            "restart synchronization from verified state",
            25,
            diagnosis));

        result.push_back(candidate(
            ActionType::CREATE_REPLAY_CASE,
            "preserve deterministic reproduction evidence",
            5,
            diagnosis));

        break;

    case IncidentType::SYNC_STALLED:
        result.push_back(candidate(
            ActionType::VERIFY_CHAIN,
            "verify local chain before retrying sync",
            5,
            diagnosis));

        result.push_back(candidate(
            ActionType::ABORT_SYNC,
            "terminate the stalled synchronization attempt",
            15,
            diagnosis));

        result.push_back(candidate(
            ActionType::RECONNECT_PEER,
            "re-establish the peer transport",
            20,
            diagnosis));

        result.push_back(candidate(
            ActionType::RESTART_SYNC,
            "restart synchronization from verified state",
            25,
            diagnosis));

        result.push_back(candidate(
            ActionType::CREATE_REPLAY_CASE,
            "preserve deterministic reproduction evidence",
            5,
            diagnosis));

        break;

    case IncidentType::UTXO_INCONSISTENCY:
        result.push_back(candidate(
            ActionType::VERIFY_CHAIN,
            "verify chain before deriving replacement UTXO state",
            5,
            diagnosis));

        result.push_back(candidate(
            ActionType::REBUILD_UTXO,
            "rebuild derived UTXO state from verified chain data",
            30,
            diagnosis));

        result.push_back(candidate(
            ActionType::CREATE_REPLAY_CASE,
            "preserve the inconsistency as a replay case",
            5,
            diagnosis));

        break;

    case IncidentType::STORAGE_FAILURE:
        result.push_back(candidate(
            ActionType::VERIFY_CHAIN,
            "verify readable chain state before recovery",
            5,
            diagnosis));

        result.push_back(candidate(
            ActionType::RECOVER_STORAGE,
            "perform bounded storage recovery",
            45,
            diagnosis));

        result.push_back(candidate(
            ActionType::CREATE_REPLAY_CASE,
            "preserve storage failure evidence",
            5,
            diagnosis));

        break;

    case IncidentType::PROCESS_CRASH:
        result.push_back(candidate(
            ActionType::RESTART_NODE,
            "restart the node under crash-loop limits",
            35,
            diagnosis));

        result.push_back(candidate(
            ActionType::CREATE_REPLAY_CASE,
            "preserve crash evidence for reproduction",
            5,
            diagnosis));

        break;

    case IncidentType::UNKNOWN:
        result.push_back(candidate(
            ActionType::VERIFY_CHAIN,
            "perform read-only chain verification",
            5,
            diagnosis));

        result.push_back(candidate(
            ActionType::CREATE_REPLAY_CASE,
            "preserve unknown failure for later diagnosis",
            5,
            diagnosis));

        break;
    }

    return result;
}

std::vector<VerificationStep>
plan_verification(const Diagnosis& diagnosis) {

    std::vector<VerificationStep> result;

    result.push_back({
        "CHAIN_INTEGRITY",
        "verify block linkage, heights, hashes and consensus state"
    });

    switch (diagnosis.type) {
    case IncidentType::INVALID_PEER_DATA:
        result.push_back({
            "PEER_QUARANTINE",
            "confirm offending peer is isolated"
        });
        result.push_back({
            "SYNC_REPLAY",
            "replay the synchronization request from verified state"
        });
        break;

    case IncidentType::SYNC_STALLED:
        result.push_back({
            "SYNC_PROGRESS",
            "confirm synchronization advances monotonically"
        });
        break;

    case IncidentType::UTXO_INCONSISTENCY:
        result.push_back({
            "UTXO_REBUILD",
            "compare rebuilt UTXO state against chain-derived state"
        });
        break;

    case IncidentType::STORAGE_FAILURE:
        result.push_back({
            "STORAGE_INTEGRITY",
            "verify recovered storage before accepting new blocks"
        });
        break;

    case IncidentType::PROCESS_CRASH:
        result.push_back({
            "CRASH_LOOP",
            "confirm restart remains below configured crash-loop threshold"
        });
        break;

    case IncidentType::UNKNOWN:
        result.push_back({
            "REPLAY_CAPTURE",
            "confirm deterministic reproduction data was preserved"
        });
        break;
    }

    return result;
}

}
