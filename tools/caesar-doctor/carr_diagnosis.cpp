#include "carr_diagnosis.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace caesar::carr {
namespace {

std::string lower(std::string value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

    return value;
}

std::string combined(const DiagnosisInput& input) {
    std::ostringstream out;

    for (const auto& signal : input.signals)
        out << signal << '\n';

    out << input.stderr_text << '\n';
    out << input.stdout_text << '\n';

    return lower(out.str());
}

bool contains(
    const std::string& text,
    const std::string& token) {
    return text.find(token) != std::string::npos;
}

}

Diagnosis diagnose(const DiagnosisInput& input) {
    const std::string text = combined(input);

    Diagnosis result;
    result.type = input.hinted_type;

    if (contains(text,
                 "received headers have invalid internal link") ||
        contains(text,
                 "invalid internal header") ||
        contains(text,
                 "previous_hash link")) {

        result.type = IncidentType::INVALID_PEER_DATA;
        result.code = "BAD_PEER_HEADER_CHAIN";
        result.root_cause =
            "peer supplied headers whose internal previous_hash link is invalid";
        result.confidence = 0.99;
        result.evidence.push_back(
            "header internal-link validation failure");

        return result;
    }

    if (contains(text,
                 "relayed block failed consensus validation") ||
        contains(text,
                 "consensus_fail") ||
        contains(text,
                 "invalid relayed block")) {

        result.type = IncidentType::INVALID_PEER_DATA;
        result.code = "INVALID_PEER_BLOCK";
        result.root_cause =
            "peer supplied a block rejected by local consensus validation";
        result.confidence = 0.98;
        result.evidence.push_back(
            "local consensus validation rejected peer data");

        return result;
    }

    if (contains(text, "sync stalled") ||
        contains(text, "sync timeout") ||
        contains(text, "synchronization timeout")) {

        result.type = IncidentType::SYNC_STALLED;
        result.code = "SYNC_STALLED";
        result.root_cause =
            "chain synchronization stopped making verified progress";
        result.confidence = 0.94;
        result.evidence.push_back(
            "synchronization progress timeout");

        return result;
    }

    if (contains(text, "utxo") &&
        (contains(text, "inconsisten") ||
         contains(text, "mismatch") ||
         contains(text, "corrupt"))) {

        result.type = IncidentType::UTXO_INCONSISTENCY;
        result.code = "UTXO_STATE_INCONSISTENCY";
        result.root_cause =
            "derived UTXO state is inconsistent with the verified chain";
        result.confidence = 0.92;
        result.evidence.push_back(
            "UTXO consistency signal");

        return result;
    }

    if (contains(text, "storage") &&
        (contains(text, "fail") ||
         contains(text, "corrupt") ||
         contains(text, "truncat") ||
         contains(text, "io error"))) {

        result.type = IncidentType::STORAGE_FAILURE;
        result.code = "STORAGE_FAILURE";
        result.root_cause =
            "blockchain storage reported an I/O or integrity failure";
        result.confidence = 0.93;
        result.evidence.push_back(
            "storage integrity or I/O signal");

        return result;
    }

    if (contains(text, "segmentation fault") ||
        contains(text, "signal 11") ||
        contains(text, "exit code 139") ||
        contains(text, "exit=139")) {

        result.type = IncidentType::PROCESS_CRASH;
        result.code = "PROCESS_CRASH";
        result.root_cause =
            "caesard process terminated abnormally";
        result.confidence = 0.99;
        result.evidence.push_back(
            "process termination signal");

        return result;
    }

    result.type = IncidentType::UNKNOWN;
    result.code = "UNKNOWN_FAILURE";
    result.root_cause =
        "available evidence does not establish a safe causal classification";
    result.confidence = 0.20;
    result.evidence.push_back(
        "no deterministic diagnosis rule matched");

    return result;
}

std::string make_fingerprint(
    const DiagnosisInput& input,
    const Diagnosis& diagnosis) {

    std::ostringstream out;

    out << static_cast<int>(diagnosis.type);
    out << '|';
    out << diagnosis.code;
    out << '|';

    for (const auto& signal : input.signals) {
        out << signal;
        out << '|';
    }

    return out.str();
}

}
