#include "carr_types.hpp"

namespace caesar::carr {

std::string to_string(IncidentType value) {
    switch (value) {
    case IncidentType::UNKNOWN: return "UNKNOWN";
    case IncidentType::PROCESS_CRASH: return "PROCESS_CRASH";
    case IncidentType::INVALID_PEER_DATA: return "INVALID_PEER_DATA";
    case IncidentType::SYNC_STALLED: return "SYNC_STALLED";
    case IncidentType::UTXO_INCONSISTENCY: return "UTXO_INCONSISTENCY";
    case IncidentType::STORAGE_FAILURE: return "STORAGE_FAILURE";
    }

    return "UNKNOWN";
}

std::string to_string(IncidentState value) {
    switch (value) {
    case IncidentState::DETECTED: return "DETECTED";
    case IncidentState::CAPTURED: return "CAPTURED";
    case IncidentState::DIAGNOSED: return "DIAGNOSED";
    case IncidentState::PLANNED: return "PLANNED";
    case IncidentState::VERIFYING: return "VERIFYING";
    case IncidentState::RECOVERED: return "RECOVERED";
    case IncidentState::ESCALATED: return "ESCALATED";
    }

    return "DETECTED";
}

std::string to_string(Severity value) {
    switch (value) {
    case Severity::INFO: return "INFO";
    case Severity::WARNING: return "WARNING";
    case Severity::ERROR: return "ERROR";
    case Severity::CRITICAL: return "CRITICAL";
    }

    return "ERROR";
}

std::string to_string(ActionType value) {
    switch (value) {
    case ActionType::NONE: return "NONE";
    case ActionType::VERIFY_CHAIN: return "VERIFY_CHAIN";
    case ActionType::QUARANTINE_PEER: return "QUARANTINE_PEER";
    case ActionType::ABORT_SYNC: return "ABORT_SYNC";
    case ActionType::RECONNECT_PEER: return "RECONNECT_PEER";
    case ActionType::RESTART_SYNC: return "RESTART_SYNC";
    case ActionType::REBUILD_UTXO: return "REBUILD_UTXO";
    case ActionType::RECOVER_STORAGE: return "RECOVER_STORAGE";
    case ActionType::RESTART_P2P: return "RESTART_P2P";
    case ActionType::RESTART_NODE: return "RESTART_NODE";
    case ActionType::CREATE_REPLAY_CASE: return "CREATE_REPLAY_CASE";
    case ActionType::CONSENSUS_CHANGE_REQUIRES_GATE:
        return "CONSENSUS_CHANGE_REQUIRES_GATE";
    }

    return "NONE";
}

}
