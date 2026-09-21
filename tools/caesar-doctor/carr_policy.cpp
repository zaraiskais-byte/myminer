#include "carr_policy.hpp"

namespace caesar::carr {

bool touches_consensus(ActionType action) {
    return action ==
        ActionType::CONSENSUS_CHANGE_REQUIRES_GATE;
}

bool autonomous_allowed(ActionType action) {
    if (touches_consensus(action))
        return false;

    switch (action) {
    case ActionType::VERIFY_CHAIN:
    case ActionType::QUARANTINE_PEER:
    case ActionType::ABORT_SYNC:
    case ActionType::RECONNECT_PEER:
    case ActionType::RESTART_SYNC:
    case ActionType::REBUILD_UTXO:
    case ActionType::RECOVER_STORAGE:
    case ActionType::RESTART_P2P:
    case ActionType::RESTART_NODE:
    case ActionType::CREATE_REPLAY_CASE:
        return true;

    case ActionType::NONE:
    case ActionType::CONSENSUS_CHANGE_REQUIRES_GATE:
        return false;
    }

    return false;
}

bool policy_allows(
    const Diagnosis& diagnosis,
    ActionType action) {

    if (!autonomous_allowed(action))
        return false;

    switch (diagnosis.type) {
    case IncidentType::INVALID_PEER_DATA:
        return action == ActionType::QUARANTINE_PEER ||
               action == ActionType::ABORT_SYNC ||
               action == ActionType::VERIFY_CHAIN ||
               action == ActionType::RESTART_SYNC ||
               action == ActionType::CREATE_REPLAY_CASE;

    case IncidentType::SYNC_STALLED:
        return action == ActionType::VERIFY_CHAIN ||
               action == ActionType::ABORT_SYNC ||
               action == ActionType::RECONNECT_PEER ||
               action == ActionType::RESTART_SYNC ||
               action == ActionType::CREATE_REPLAY_CASE;

    case IncidentType::UTXO_INCONSISTENCY:
        return action == ActionType::VERIFY_CHAIN ||
               action == ActionType::REBUILD_UTXO ||
               action == ActionType::CREATE_REPLAY_CASE;

    case IncidentType::STORAGE_FAILURE:
        return action == ActionType::VERIFY_CHAIN ||
               action == ActionType::RECOVER_STORAGE ||
               action == ActionType::CREATE_REPLAY_CASE;

    case IncidentType::PROCESS_CRASH:
        return action == ActionType::RESTART_NODE ||
               action == ActionType::CREATE_REPLAY_CASE;

    case IncidentType::UNKNOWN:
        return action == ActionType::VERIFY_CHAIN ||
               action == ActionType::CREATE_REPLAY_CASE;
    }

    return false;
}

}
