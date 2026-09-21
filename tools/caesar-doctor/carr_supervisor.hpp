#pragma once

#include "carr_runtime.hpp"

#include <cstdint>
#include <filesystem>
#include <string>

namespace caesar::carr {

struct SupervisorResult {
    bool started = false;
    bool ready = false;
    bool recovered = false;
    bool quarantined = false;
    bool stopped = false;

    std::uint32_t restart_count = 0;

    int last_exit_code = 0;
    int last_signal = 0;

    std::string execution_id;
    std::string status;
    std::string detail;
};

class CARRSupervisor {
public:
    CARRSupervisor(
        std::filesystem::path root,
        RuntimeLimits limits = {});

    SupervisorResult run(
        const RuntimeCommand& command);

    SupervisorResult start(
        const RuntimeCommand& command);

    SupervisorResult poll();

    SupervisorResult stop(
        int signal_number = 15);

    SupervisorResult wait(
        std::uint32_t timeout_ms = 5000);

    bool running() const;
    bool ready() const;

private:
    std::filesystem::path root_;
    RuntimeLimits limits_;

    int process_id_ = -1;
    bool started_ = false;
    bool ready_ = false;
    bool stopped_ = false;

    std::string execution_id_;


    SupervisorResult run_once(
        const RuntimeCommand& command,
        std::uint32_t restart_count);
};

}
