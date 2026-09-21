#pragma once

#include "carr_types.hpp"

#include <filesystem>
#include <string>

namespace caesar::carr {

std::string make_incident_id();

void append_incident_json(
    const IncidentRecord& record,
    const std::filesystem::path& root);

}
