#include "carr_engine.hpp"
#include "carr_runtime.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(
    bool condition,
    const char* message) {

    if (!condition)
        throw std::runtime_error(message);
}

caesar::carr::IncidentRecord make_crash_incident(
    caesar::carr::Engine& engine) {

    caesar::carr::DiagnosisInput input;

    input.signals.push_back(
        "segmentation fault");

    return engine.process(input);
}

}

int main() {
    try {
        const char* root_env =
            std::getenv(
                "CAESAR_CARR_RUNTIME_TEST_ROOT");

        if (root_env == nullptr ||
            *root_env == '\0') {

            throw std::runtime_error(
                "CAESAR_CARR_RUNTIME_TEST_ROOT is not set");
        }

        const std::filesystem::path root =
            root_env;

        std::filesystem::remove_all(root);

        caesar::carr::Engine engine(root);

        const auto incident =
            make_crash_incident(engine);

        caesar::carr::RuntimeLimits limits;
        limits.max_restarts = 2;
        limits.restart_delay_ms = 1;

        caesar::carr::RuntimeExecutor executor(
            root,
            limits);

        caesar::carr::RuntimeCommand crash_command;
        crash_command.executable = "sh";
        crash_command.arguments = {
            "-c",
            "kill -SEGV $$"
        };
        crash_command.working_directory = root;

        const auto blocked =
            executor.execute(
                incident,
                caesar::carr::ActionType::
                    CONSENSUS_CHANGE_REQUIRES_GATE,
                crash_command);

        require(
            !blocked.allowed,
            "consensus action was allowed");

        require(
            blocked.status ==
                "POLICY_BLOCKED",
            "consensus policy status failed");

        const auto recovered =
            executor.execute(
                incident,
                caesar::carr::ActionType::
                    RESTART_NODE,
                caesar::carr::RuntimeCommand{
                    "sh",
                    {"-c", "exit 0"},
                    root
                });

        require(
            recovered.allowed,
            "restart action was not allowed");

        require(
            recovered.executed,
            "restart action was not executed");

        require(
            recovered.verified,
            "successful restart was not verified");

        require(
            recovered.status ==
                "RECOVERED",
            "successful restart status failed");

        require(
            !recovered.execution_id.empty(),
            "successful restart execution id missing");

        require(
            recovered.exit_code == 0,
            "successful restart exit code failed");

        const auto quarantined =
            executor.execute(
                incident,
                caesar::carr::ActionType::
                    RESTART_NODE,
                crash_command);

        require(
            quarantined.allowed,
            "crash-loop restart was not allowed");

        require(
            quarantined.executed,
            "crash-loop action was not executed");

        require(
            !quarantined.verified,
            "crash-loop action was incorrectly verified");

        require(
            quarantined.status ==
                "CRASH_LOOP_QUARANTINE",
            "crash-loop quarantine status failed");

        require(
            quarantined.restart_count ==
                limits.max_restarts,
            "crash-loop restart count failed");

        require(
            quarantined.signal_number != 0,
            "crash-loop signal capture failed");

        require(
            !quarantined.execution_id.empty(),
            "crash-loop execution id missing");

        const auto replay =
            executor.execute(
                incident,
                caesar::carr::ActionType::
                    CREATE_REPLAY_CASE,
                crash_command);

        require(
            replay.allowed,
            "replay action was not allowed");

        require(
            replay.executed,
            "replay action was not executed");

        require(
            replay.verified,
            "replay action was not verified");

        require(
            !replay.execution_id.empty(),
            "replay execution id missing");

        const auto action_dir =
            root /
            "doctor-data" /
            "carr" /
            "actions";

        const auto replay_dir =
            root /
            "doctor-data" /
            "carr" /
            "replay";

        require(
            std::filesystem::exists(
                action_dir),
            "action journal missing");

        require(
            std::filesystem::exists(
                replay_dir),
            "replay directory missing");

        std::size_t action_count = 0;
        std::size_t consensus_gate_count = 0;
        std::size_t restart_node_count = 0;
        std::size_t replay_action_count = 0;

        for (const auto& entry :
             std::filesystem::directory_iterator(
                 action_dir)) {

            if (!entry.is_regular_file() ||
                entry.path().extension() !=
                    ".json") {
                continue;
            }

            ++action_count;

            const auto name =
                entry.path().filename().string();

            if (name.find(
                    "-CONSENSUS_CHANGE_REQUIRES_GATE-") !=
                std::string::npos) {
                ++consensus_gate_count;
            }

            if (name.find(
                    "-RESTART_NODE-") !=
                std::string::npos) {
                ++restart_node_count;
            }

            if (name.find(
                    "-CREATE_REPLAY_CASE-") !=
                std::string::npos) {
                ++replay_action_count;
            }
        }

        require(
            action_count == 4,
            "unexpected action journal count");

        require(
            consensus_gate_count == 1,
            "unexpected consensus gate journal count");

        require(
            restart_node_count == 2,
            "unexpected restart action journal count");

        require(
            replay_action_count == 1,
            "unexpected replay action journal count");

        std::size_t replay_count = 0;

        for (const auto& entry :
             std::filesystem::directory_iterator(
                 replay_dir)) {

            if (entry.is_regular_file() &&
                entry.path().extension() ==
                    ".json") {

                ++replay_count;
            }
        }

        require(
            replay_count == 1,
            "unexpected replay case count");

        std::cout
            << "CARR_RUNTIME_TEST=PASS\n";

        std::cout
            << "CARR_RUNTIME_ACTIONS="
            << action_count
            << "\n";

        std::cout
            << "CARR_RUNTIME_RESTARTS="
            << restart_node_count
            << "\n";

        std::cout
            << "CARR_RUNTIME_REPLAY="
            << replay_count
            << "\n";

        std::cout
            << "CARR_RUNTIME_CONSENSUS_GATE=BLOCKED\n";

        return 0;
    } catch (const std::exception& e) {
        std::cerr
            << "CARR_RUNTIME_TEST=FAIL\n"
            << e.what()
            << '\n';

        return 1;
    }
}
