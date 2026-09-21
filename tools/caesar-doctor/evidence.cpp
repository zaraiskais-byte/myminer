#include "evidence.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <sys/types.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace caesar::doctor {

namespace {

std::string timestamp() {
    const auto now =
        std::chrono::system_clock::now();

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

std::string shell_output(
    const std::string& command) {

    std::string result;

    FILE* pipe =
        popen(command.c_str(), "r");

    if (!pipe)
        return result;

    char buffer[512];

    while (fgets(
        buffer,
        sizeof(buffer),
        pipe)) {

        result += buffer;
    }

    pclose(pipe);

    while (!result.empty() &&
           (result.back() == '\n' ||
            result.back() == '\r')) {
        result.pop_back();
    }

    return result;
}

std::vector<std::string>
lines(const std::string& text) {

    std::vector<std::string> result;
    std::istringstream input(text);
    std::string line;

    while (std::getline(input, line)) {
        if (!line.empty())
            result.push_back(line);
    }

    return result;
}

}

EvidenceSnapshot collect_evidence(
    const std::string& repository_root,
    int pid) {

    EvidenceSnapshot snapshot;

    snapshot.timestamp = timestamp();

    snapshot.process.pid = pid;

    if (pid > 0) {
        const fs::path proc_root =
            fs::path("/proc") /
            std::to_string(pid);

        const fs::path exe_path =
            proc_root / "exe";

        const fs::path cmdline_path =
            proc_root / "cmdline";

        std::error_code ec;

        if (fs::exists(exe_path, ec)) {
            snapshot.process.executable =
                shell_output(
                    "readlink " +
                    exe_path.string() +
                    " 2>/dev/null");
        }

        ec.clear();

        if (fs::exists(cmdline_path, ec)) {
            snapshot.process.command_line =
                shell_output(
                    "tr '\\0' ' ' < " +
                    cmdline_path.string() +
                    " 2>/dev/null");
        }
    }

    snapshot.repository.root =
        repository_root;

    snapshot.repository.head =
        shell_output(
            "cd \"" +
            repository_root +
            "\" && git rev-parse HEAD 2>/dev/null");

    const auto status =
        shell_output(
            "cd \"" +
            repository_root +
            "\" && git status --porcelain 2>/dev/null");

    for (const auto& line :
         lines(status)) {

        if (line.size() >= 2 &&
            line[0] == '?' &&
            line[1] == '?') {

            snapshot.repository.untracked.push_back(
                line.substr(3));

        } else if (line.size() >= 2) {

            snapshot.repository.modified.push_back(
                line);
        }
    }

    const fs::path root =
        repository_root;

    snapshot.build.cmake_present =
        fs::exists(
            root / "CMakeLists.txt");

    snapshot.build.build_directory_present =
        fs::exists(
            root / "build");

    snapshot.build.caesard_present =
        fs::exists(
            root / "caesard");

    return snapshot;
}

std::string to_json(
    const EvidenceSnapshot& snapshot) {

    std::ostringstream out;

    out << "{\n";
    out << "  \"timestamp\": \""
        << snapshot.timestamp
        << "\",\n";

    out << "  \"process\": {\n";
    out << "    \"pid\": "
        << snapshot.process.pid
        << ",\n";
    out << "    \"executable\": \""
        << snapshot.process.executable
        << "\",\n";
    out << "    \"command_line\": \""
        << snapshot.process.command_line
        << "\"\n";
    out << "  },\n";

    out << "  \"repository\": {\n";
    out << "    \"root\": \""
        << snapshot.repository.root
        << "\",\n";
    out << "    \"head\": \""
        << snapshot.repository.head
        << "\",\n";

    out << "    \"modified\": [";

    for (std::size_t i = 0;
         i < snapshot.repository.modified.size();
         ++i) {

        if (i)
            out << ", ";

        out << "\""
            << snapshot.repository.modified[i]
            << "\"";
    }

    out << "],\n";

    out << "    \"untracked\": [";

    for (std::size_t i = 0;
         i < snapshot.repository.untracked.size();
         ++i) {

        if (i)
            out << ", ";

        out << "\""
            << snapshot.repository.untracked[i]
            << "\"";
    }

    out << "]\n";
    out << "  },\n";

    out << "  \"build\": {\n";
    out << "    \"cmake_present\": "
        << (snapshot.build.cmake_present
            ? "true" : "false")
        << ",\n";
    out << "    \"build_directory_present\": "
        << (snapshot.build.build_directory_present
            ? "true" : "false")
        << ",\n";
    out << "    \"caesard_present\": "
        << (snapshot.build.caesard_present
            ? "true" : "false")
        << "\n";
    out << "  }\n";

    out << "}\n";

    return out.str();
}

}
