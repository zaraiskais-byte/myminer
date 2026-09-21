#pragma once

#include "carr_diagnosis.hpp"
#include "carr_types.hpp"

#include <filesystem>

namespace caesar::carr {

class Engine {
public:
    explicit Engine(
        std::filesystem::path repository_root);

    IncidentRecord process(
        const DiagnosisInput& input);

private:
    std::filesystem::path repository_root_;
};

}
