#pragma once

#include "carr_types.hpp"

namespace caesar::carr {

bool autonomous_allowed(ActionType action);

bool touches_consensus(ActionType action);

bool policy_allows(
    const Diagnosis& diagnosis,
    ActionType action);

}
