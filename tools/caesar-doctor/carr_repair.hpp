#pragma once

#include "carr_types.hpp"

#include <vector>

namespace caesar::carr {

std::vector<RepairCandidate>
plan_repairs(const Diagnosis& diagnosis);

std::vector<VerificationStep>
plan_verification(const Diagnosis& diagnosis);

}
