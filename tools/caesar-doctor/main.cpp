#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "evidence.hpp"

namespace fs = std::filesystem;

namespace {

struct Policy {
    int max_restarts = 3;
    int cooldown_seconds = 5;
    int crash_window_seconds = 60;
};

struct Incident {
    long long id = 0;
    int pid = 0;
    int exit_code = -1;
    int signal = 0;
    int restart_number = 0;
    std::string reason;
    std::string timestamp;
    caesar::doctor::EvidenceSnapshot evidence;
};

std::string utc_timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto value =
        std::chrono::system_clock::to_time_t(now);

    char buffer[64]{};

    std::tm tm{};
    gmtime_r(&value, &tm);

    std::strftime(
        buffer,
        sizeof(buffer),
        "%Y-%m-%dT%H:%M:%SZ",
        &tm);

    return buffer;
}

long long unix_millis() {
    const auto now =
        std::chrono::system_clock::now();

    return std::chrono::duration_cast<
        std::chrono::milliseconds>(
        now.time_since_epoch())
        .count();
}

void write_incident(
    const fs::path& directory,
    const Incident& incident) {

    fs::create_directories(directory);

    const auto path =
        directory /
        ("incident-" +
         std::to_string(incident.id) +
         ".json");

    std::ofstream out(path);

    if (!out)
        throw std::runtime_error(
            "cannot create incident file");

    out << "{\n";
    out << "  \"id\": "
        << incident.id << ",\n";
    out << "  \"pid\": "
        << incident.pid << ",\n";
    out << "  \"exit_code\": "
        << incident.exit_code << ",\n";
    out << "  \"signal\": "
        << incident.signal << ",\n";
    out << "  \"restart_number\": "
        << incident.restart_number << ",\n";
    out << "  \"reason\": \""
        << incident.reason << "\",\n";
    out << "  \"timestamp\": \""
        << incident.timestamp << "\",\n";

    out << "  \"evidence\": "
        << caesar::doctor::to_json(
               incident.evidence)
        << "\n";

    out << "}\n";
}

struct ChildResult {
    pid_t pid = -1;
    int exit_code = -1;
    int signal = 0;
    bool exited = false;
    bool signaled = false;
    caesar::doctor::ProcessEvidence process;
};

ChildResult run_child(
    const std::vector<std::string>& command,
    const fs::path& stdout_path,
    const fs::path& stderr_path) {

    pid_t pid = fork();

    if (pid < 0)
        throw std::runtime_error(
            "fork failed");

    if (pid == 0) {
        FILE* stdout_file =
            std::fopen(
                stdout_path.c_str(),
                "a");

        FILE* stderr_file =
            std::fopen(
                stderr_path.c_str(),
                "a");

        if (!stdout_file ||
            !stderr_file) {
            _exit(127);
        }

        ::dup2(
            fileno(stdout_file),
            STDOUT_FILENO);

        ::dup2(
            fileno(stderr_file),
            STDERR_FILENO);

        std::fclose(stdout_file);
        std::fclose(stderr_file);

        std::vector<char*> argv;
        argv.reserve(command.size() + 1);

        for (const auto& argument : command)
            argv.push_back(
                const_cast<char*>(
                    argument.c_str()));

        argv.push_back(nullptr);

        ::execvp(
            argv[0],
            argv.data());

        _exit(127);
    }

    caesar::doctor::ProcessEvidence process;
    process.pid = static_cast<int>(pid);

    if (!command.empty()) {
        process.executable = command.front();

        for (std::size_t i = 0; i < command.size(); ++i) {
            if (i != 0)
                process.command_line += " ";
            process.command_line += command[i];
        }
    }

    int status = 0;

    while (true) {
        const pid_t result =
            waitpid(pid, &status, 0);

        if (result == pid)
            break;

        if (result < 0) {
            if (errno == EINTR)
                continue;

            throw std::runtime_error(
                "waitpid failed");
        }
    }

    ChildResult result;
    result.pid = pid;
    result.process = process;

    if (WIFEXITED(status)) {
        result.exited = true;
        result.exit_code = WEXITSTATUS(status);
        return result;
    }

    if (WIFSIGNALED(status)) {
        result.signaled = true;
        result.signal = WTERMSIG(status);
        result.exit_code = 128 + result.signal;
        return result;
    }

    result.exit_code = 255;
    return result;
}

void self_test() {
    const fs::path root =
        fs::temp_directory_path() /
        "caesar-doctor-self-test";

    std::error_code ec;
    fs::remove_all(root, ec);

    const auto incident_dir =
        root / "incidents";

    Incident incident;
    incident.id = unix_millis();
    incident.pid = 12345;
    incident.exit_code = 139;
    incident.signal = SIGSEGV;
    incident.restart_number = 1;
    incident.reason = "self-test";
    incident.timestamp = utc_timestamp();

    incident.evidence =
        caesar::doctor::collect_evidence(
            fs::current_path().string(),
            static_cast<int>(getpid()));

    write_incident(
        incident_dir,
        incident);

    if (!fs::exists(incident_dir))
        throw std::runtime_error(
            "self-test directory missing");

    bool found = false;

    for (const auto& entry :
         fs::directory_iterator(incident_dir)) {

        if (entry.is_regular_file()) {
            found = true;
            break;
        }
    }

    if (!found)
        throw std::runtime_error(
            "self-test incident missing");

    fs::remove_all(root, ec);

    std::cout
        << "CAESAR_DOCTOR_SELF_TEST=PASS\n";
}

void usage(const char* program) {
    std::cerr
        << "Usage:\n"
        << "  " << program
        << " --self-test\n"
        << "  " << program
        << " --watch <command> [args...]\n";
}

int watch(
    const std::vector<std::string>& command) {

    if (command.empty())
        throw std::runtime_error(
            "empty command");

    const fs::path root =
        fs::current_path() /
        "doctor-data";

    const fs::path incidents =
        root / "incidents";

    const fs::path logs =
        root / "logs";

    fs::create_directories(
        incidents);

    fs::create_directories(
        logs);

    Policy policy;

    int restart_count = 0;

    const auto started =
        std::chrono::steady_clock::now();

    while (true) {
        const auto stdout_path =
            logs / "caesard.stdout.log";

        const auto stderr_path =
            logs / "caesard.stderr.log";

        const auto before =
            std::chrono::steady_clock::now();

        const ChildResult child =
            run_child(
                command,
                stdout_path,
                stderr_path);

        const auto after =
            std::chrono::steady_clock::now();

        const int rc =
            child.exit_code;

        const auto runtime =
            std::chrono::duration_cast<
                std::chrono::seconds>(
                after - before)
                .count();

        const bool abnormal =
            rc != 0;

        if (!abnormal) {
            std::cout
                << "CAESAR_DOCTOR="
                << "PROCESS_EXITED_CLEANLY\n";
            return 0;
        }

        if (runtime >=
            policy.crash_window_seconds) {
            restart_count = 0;
        }

        ++restart_count;

        Incident incident;
        incident.id = unix_millis();
        incident.pid =
            static_cast<int>(child.pid);
        incident.exit_code = rc;
        incident.signal = child.signal;
        incident.restart_number =
            restart_count;
        incident.reason =
            "abnormal-process-exit";
        incident.timestamp =
            utc_timestamp();

        incident.evidence =
            caesar::doctor::collect_evidence(
                fs::current_path().string(),
                static_cast<int>(child.pid));

        incident.evidence.process =
            child.process;

        write_incident(
            incidents,
            incident);

        std::cerr
            << "CAESAR_DOCTOR="
            << "INCIDENT"
            << " exit_code=" << rc
            << " restart="
            << restart_count
            << "\n";

        if (restart_count >
            policy.max_restarts) {

            std::cerr
                << "CAESAR_DOCTOR="
                << "CRASH_LOOP_QUARANTINE\n";

            return 70;
        }

        std::this_thread::sleep_for(
            std::chrono::seconds(
                policy.cooldown_seconds));
    }
}

}

int main(int argc, char** argv) {
    try {
        if (argc == 2 &&
            std::string(argv[1]) ==
                "--self-test") {

            self_test();
            return 0;
        }

        if (argc >= 3 &&
            std::string(argv[1]) ==
                "--watch") {

            std::vector<std::string> command;

            for (int i = 2; i < argc; ++i)
                command.emplace_back(
                    argv[i]);

            return watch(command);
        }

        usage(argv[0]);
        return 2;

    } catch (const std::exception& e) {
        std::cerr
            << "CAESAR_DOCTOR_FATAL="
            << e.what()
            << "\n";

        return 1;
    }
}
