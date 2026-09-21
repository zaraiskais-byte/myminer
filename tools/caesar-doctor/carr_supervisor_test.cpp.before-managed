#include "carr_supervisor.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>

using namespace caesar::carr;

int main() {
    const auto root =
        std::filesystem::path(
            "build-carr-v1/supervisor-test-runtime");

    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    RuntimeLimits limits;
    limits.max_restarts = 2;
    limits.restart_delay_ms = 10;
    limits.startup_grace_ms = 100;
    limits.monitor_interval_ms = 10;

    CARRSupervisor supervisor(root, limits);

    RuntimeCommand healthy;
    healthy.executable = "/bin/sh";
    healthy.arguments = {
        "-c",
        "sleep 1"
    };

    const auto healthy_result =
        supervisor.run(healthy);

    assert(healthy_result.started);
    assert(healthy_result.recovered);
    assert(!healthy_result.quarantined);
    assert(healthy_result.status == "RECOVERED");
    assert(healthy_result.last_exit_code == 0);
    assert(!healthy_result.execution_id.empty());

    RuntimeCommand crash;
    crash.executable = "/bin/sh";
    crash.arguments = {
        "-c",
        "kill -SEGV $$"
    };

    const auto crash_result =
        supervisor.run(crash);

    assert(crash_result.started);
    assert(crash_result.quarantined);
    assert(!crash_result.recovered);
    assert(crash_result.restart_count == limits.max_restarts);
    assert(crash_result.last_signal != 0);
    assert(!crash_result.execution_id.empty());

    std::cout
        << "CARR_SUPERVISOR_START=PASS\n"
        << "CARR_SUPERVISOR_RUNNING=PASS\n"
        << "CARR_SUPERVISOR_SIGNAL=PASS\n"
        << "CARR_SUPERVISOR_RESTART=PASS\n"
        << "CARR_SUPERVISOR_QUARANTINE=PASS\n";

    return 0;
}
