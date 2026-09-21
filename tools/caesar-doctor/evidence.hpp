#pragma once

#include <string>
#include <vector>

namespace caesar::doctor {

struct ProcessEvidence {
    int pid = 0;
    std::string executable;
    std::string command_line;
};

struct RepositoryEvidence {
    std::string root;
    std::string head;
    std::vector<std::string> modified;
    std::vector<std::string> untracked;
};

struct BuildEvidence {
    bool cmake_present = false;
    bool build_directory_present = false;
    bool caesard_present = false;
};

struct EvidenceSnapshot {
    std::string timestamp;
    ProcessEvidence process;
    RepositoryEvidence repository;
    BuildEvidence build;
};

EvidenceSnapshot collect_evidence(
    const std::string& repository_root,
    int pid);

std::string to_json(
    const EvidenceSnapshot& snapshot);

}
