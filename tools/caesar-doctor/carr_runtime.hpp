#pragma once

#include "carr_types.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace caesar::carr {

struct RuntimeCommand {
    std::string executable;
    std::vector<std::string> arguments;
    std::filesystem::path working_directory;
};

struct RuntimeLimits {
    std::uint32_t max_restarts = 3;
    std::uint32_t crash_window_seconds = 60;
    std::uint32_t restart_delay_ms = 250;
    std::uint32_t startup_grace_ms = 500;
    std::uint32_t monitor_interval_ms = 100;
};

struct RuntimeResult {
    bool success = false;
    int exit_code = -1;
    int signal = 0;
    std::uint32_t restart_count = 0;
    std::string status;
};

struct ActionExecution {
    bool allowed = false;
    bool executed = false;
    bool verified = false;
    ActionType action = ActionType::NONE;
    std::string execution_id;
    std::string status;
    std::string detail;
    int restart_count = 0;
    int signal_number = 0;
    int exit_code = 0;
};

class RuntimeExecutor {
public:
    RuntimeExecutor(
        std::filesystem::path root,
        RuntimeLimits limits = {});

    ActionExecution execute(
        const IncidentRecord& incident,
        ActionType action,
        const RuntimeCommand& command);

private:
    std::filesystem::path root_;
    RuntimeLimits limits_;

    ActionExecution execute_restart_node(
        const IncidentRecord& incident,
        const RuntimeCommand& command);

    ActionExecution execute_replay_case(
        const IncidentRecord& incident);

    void append_action_json(
        const IncidentRecord& incident,
        const ActionExecution& execution) const;

    void write_replay_case(
        const IncidentRecord& incident) const;
};

}
