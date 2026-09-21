#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace caesar::carr {

enum class IncidentType {
    UNKNOWN,
    PROCESS_CRASH,
    INVALID_PEER_DATA,
    SYNC_STALLED,
    UTXO_INCONSISTENCY,
    STORAGE_FAILURE
};

enum class IncidentState {
    DETECTED,
    CAPTURED,
    DIAGNOSED,
    PLANNED,
    VERIFYING,
    RECOVERED,
    ESCALATED
};

enum class Severity {
    INFO,
    WARNING,
    ERROR,
    CRITICAL
};

enum class ActionType {
    NONE,
    VERIFY_CHAIN,
    QUARANTINE_PEER,
    ABORT_SYNC,
    RECONNECT_PEER,
    RESTART_SYNC,
    REBUILD_UTXO,
    RECOVER_STORAGE,
    RESTART_P2P,
    RESTART_NODE,
    CREATE_REPLAY_CASE,
    CONSENSUS_CHANGE_REQUIRES_GATE
};

struct Symptom {
    std::string code;
    std::string detail;
};

struct Diagnosis {
    IncidentType type = IncidentType::UNKNOWN;
    std::string code;
    std::string root_cause;
    double confidence = 0.0;
    std::vector<std::string> evidence;
};

struct RepairCandidate {
    ActionType action = ActionType::NONE;
    std::string rationale;
    int risk = 100;
    bool autonomous = false;
};

struct VerificationStep {
    std::string code;
    std::string description;
};

struct IncidentRecord {
    std::string incident_id;
    std::int64_t created_unix_ms = 0;
    IncidentType type = IncidentType::UNKNOWN;
    IncidentState state = IncidentState::DETECTED;
    Severity severity = Severity::ERROR;
    std::string fingerprint;

    std::vector<Symptom> symptoms;
    Diagnosis diagnosis;
    std::vector<RepairCandidate> candidates;
    std::vector<VerificationStep> verification;
};

std::string to_string(IncidentType value);
std::string to_string(IncidentState value);
std::string to_string(Severity value);
std::string to_string(ActionType value);

}
