#pragma once

#include "carr_types.hpp"

#include <string>
#include <vector>

namespace caesar::carr {

struct DiagnosisInput {
    IncidentType hinted_type = IncidentType::UNKNOWN;
    std::vector<std::string> signals;
    std::string stderr_text;
    std::string stdout_text;
};

Diagnosis diagnose(const DiagnosisInput& input);

std::string make_fingerprint(
    const DiagnosisInput& input,
    const Diagnosis& diagnosis);

}
