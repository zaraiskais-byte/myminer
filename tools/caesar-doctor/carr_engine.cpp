#include "carr_engine.hpp"

#include "carr_journal.hpp"
#include "carr_repair.hpp"

#include <chrono>

namespace caesar::carr {

Engine::Engine(
    std::filesystem::path repository_root)
    : repository_root_(std::move(repository_root)) {
}

IncidentRecord Engine::process(
    const DiagnosisInput& input) {

    IncidentRecord record;

    record.incident_id =
        make_incident_id();

    const auto now =
        std::chrono::system_clock::now();

    record.created_unix_ms =
        std::chrono::duration_cast<
            std::chrono::milliseconds>(
            now.time_since_epoch()).count();

    record.state =
        IncidentState::CAPTURED;

    record.severity =
        Severity::ERROR;

    record.symptoms.reserve(
        input.signals.size());

    for (const auto& signal : input.signals) {
        record.symptoms.push_back({
            "SIGNAL",
            signal
        });
    }

    record.diagnosis =
        diagnose(input);

    record.type =
        record.diagnosis.type;

    record.fingerprint =
        make_fingerprint(
            input,
            record.diagnosis);

    record.state =
        IncidentState::DIAGNOSED;

    record.candidates =
        plan_repairs(
            record.diagnosis);

    record.verification =
        plan_verification(
            record.diagnosis);

    record.state =
        IncidentState::PLANNED;

    append_incident_json(
        record,
        repository_root_);

    return record;
}

}
