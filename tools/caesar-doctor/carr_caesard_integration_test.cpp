#include "carr_supervisor.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

using namespace caesar::carr;

namespace {

void require(
    bool condition,
    const std::string& message) {

    if (!condition) {
        std::cerr
            << "FAIL: "
            << message
            << "\n";

        std::exit(1);
    }
}

class SupervisorGuard {
public:
    explicit SupervisorGuard(
        CARRSupervisor& supervisor)
        : supervisor_(supervisor) {
    }

    ~SupervisorGuard() {
        if (supervisor_.running()) {
            supervisor_.stop();
            supervisor_.wait(5000);
        }
    }

private:
    CARRSupervisor& supervisor_;
};

}

int main() {
    const char* executable =
        std::getenv("CAESARD_EXECUTABLE");

    require(
        executable != nullptr &&
        std::string(executable).size() > 0,
        "CAESARD_EXECUTABLE is required");

    const char* root_env =
        std::getenv(
            "CAESAR_CARR_CAESARD_TEST_ROOT");

    require(
        root_env != nullptr &&
        std::string(root_env).size() > 0,
        "CAESAR_CARR_CAESARD_TEST_ROOT is required");

    const std::filesystem::path root =
        root_env;

    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    RuntimeLimits limits;
    limits.startup_grace_ms = 1500;
    limits.monitor_interval_ms = 50;
    limits.max_restarts = 2;

    RuntimeCommand command;
    command.executable = executable;
    command.arguments = {
        "--data-dir",
        (root / "node-data").string(),
        "--port",
        "19555",
        "--network-id",
        "1"
    };
    command.working_directory = root;

    CARRSupervisor supervisor(
        root,
        limits);

    SupervisorGuard guard(supervisor);

    SupervisorResult started =
        supervisor.start(command);

    require(
        started.started,
        "caesard did not start");

    const auto deadline =
        std::chrono::steady_clock::now() +
        std::chrono::milliseconds(
            limits.startup_grace_ms + 1500);

    SupervisorResult running;

    while (
        std::chrono::steady_clock::now() <
        deadline) {

        running =
            supervisor.poll();

        if (running.stopped) {
            break;
        }

        if (running.ready) {
            break;
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(
                limits.monitor_interval_ms));
    }

    require(
        supervisor.running(),
        "caesard is not alive after startup");

    require(
        supervisor.ready(),
        "caesard process did not remain alive");

    const auto blockchain =
        root /
        "node-data" /
        "blockchain.dat";

    require(
        std::filesystem::exists(blockchain),
        "caesard did not create blockchain storage");

    require(
        std::filesystem::file_size(blockchain) > 0,
        "blockchain storage is empty");

    SupervisorResult stop =
        supervisor.stop();

    require(
        stop.status == "STOP_SIGNAL_SENT",
        "SIGTERM was not sent");

    SupervisorResult finished =
        supervisor.wait(5000);

    require(
        finished.stopped,
        "caesard did not stop");

    require(
        finished.last_signal == 0,
        "caesard stopped by unexpected signal");

    require(
        finished.last_exit_code == 0,
        "caesard did not exit cleanly");

    std::cout
        << "CARR_CAESARD_START=PASS\n";

    std::cout
        << "CARR_CAESARD_RUNNING=PASS\n";

    std::cout
        << "CARR_CAESARD_STORAGE=PASS\n";

    std::cout
        << "CARR_CAESARD_TERM=PASS\n";

    std::cout
        << "CARR_CAESARD_EXIT=0\n";

    return 0;
}
