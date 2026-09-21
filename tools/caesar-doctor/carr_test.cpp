#include "carr_engine.hpp"
#include "carr_policy.hpp"

#include <filesystem>
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

bool has_action(
    const caesar::carr::IncidentRecord& record,
    caesar::carr::ActionType action) {

    for (const auto& candidate :
         record.candidates) {

        if (candidate.action == action)
            return true;
    }

    return false;
}

bool has_autonomous_action(
    const caesar::carr::IncidentRecord& record,
    caesar::carr::ActionType action) {

    for (const auto& candidate :
         record.candidates) {

        if (candidate.action == action &&
            candidate.autonomous) {

            return true;
        }
    }

    return false;
}

}

int main() {
    try {
        const char* root_env =
            std::getenv("CAESAR_CARR_TEST_ROOT");

        if (root_env == nullptr ||
            *root_env == '\0') {

            throw std::runtime_error(
                "CAESAR_CARR_TEST_ROOT is not set");
        }

        const std::filesystem::path root =
            root_env;

        std::filesystem::remove_all(root);

        caesar::carr::Engine engine(root);

        {
            caesar::carr::DiagnosisInput input;

            input.signals.push_back(
                "received headers have invalid internal link");

            const auto incident =
                engine.process(input);

            require(
                incident.type ==
                    caesar::carr::IncidentType::INVALID_PEER_DATA,
                "invalid peer data diagnosis failed");

            require(
                incident.diagnosis.code ==
                    "BAD_PEER_HEADER_CHAIN",
                "header diagnosis code failed");

            require(
                incident.diagnosis.confidence > 0.98,
                "header diagnosis confidence failed");

            require(
                has_action(
                    incident,
                    caesar::carr::ActionType::QUARANTINE_PEER),
                "quarantine plan missing");

            require(
                has_autonomous_action(
                    incident,
                    caesar::carr::ActionType::RESTART_SYNC),
                "restart sync autonomous plan missing");

            require(
                !caesar::carr::autonomous_allowed(
                    caesar::carr::ActionType::
                        CONSENSUS_CHANGE_REQUIRES_GATE),
                "consensus gate was not blocked");
        }

        {
            caesar::carr::DiagnosisInput input;

            input.signals.push_back(
                "relayed block failed consensus validation");

            const auto incident =
                engine.process(input);

            require(
                incident.diagnosis.code ==
                    "INVALID_PEER_BLOCK",
                "invalid block diagnosis failed");

            require(
                has_action(
                    incident,
                    caesar::carr::ActionType::VERIFY_CHAIN),
                "chain verification plan missing");
        }

        {
            caesar::carr::DiagnosisInput input;

            input.signals.push_back(
                "sync timeout");

            const auto incident =
                engine.process(input);

            require(
                incident.type ==
                    caesar::carr::IncidentType::SYNC_STALLED,
                "sync diagnosis failed");

            require(
                has_action(
                    incident,
                    caesar::carr::ActionType::RECONNECT_PEER),
                "reconnect plan missing");

            require(
                has_action(
                    incident,
                    caesar::carr::ActionType::RESTART_SYNC),
                "restart sync plan missing");
        }

        {
            caesar::carr::DiagnosisInput input;

            input.signals.push_back(
                "utxo state inconsistency mismatch");

            const auto incident =
                engine.process(input);

            require(
                incident.type ==
                    caesar::carr::IncidentType::UTXO_INCONSISTENCY,
                "UTXO diagnosis failed");

            require(
                has_action(
                    incident,
                    caesar::carr::ActionType::REBUILD_UTXO),
                "UTXO rebuild plan missing");
        }

        {
            caesar::carr::DiagnosisInput input;

            input.signals.push_back(
                "storage io error");

            const auto incident =
                engine.process(input);

            require(
                incident.type ==
                    caesar::carr::IncidentType::STORAGE_FAILURE,
                "storage diagnosis failed");

            require(
                has_action(
                    incident,
                    caesar::carr::ActionType::RECOVER_STORAGE),
                "storage recovery plan missing");
        }

        {
            caesar::carr::DiagnosisInput input;

            input.signals.push_back(
                "segmentation fault");

            const auto incident =
                engine.process(input);

            require(
                incident.type ==
                    caesar::carr::IncidentType::PROCESS_CRASH,
                "crash diagnosis failed");

            require(
                has_autonomous_action(
                    incident,
                    caesar::carr::ActionType::RESTART_NODE),
                "node restart plan missing");
        }

        {
            caesar::carr::DiagnosisInput input;

            input.signals.push_back(
                "unclassified failure");

            const auto incident =
                engine.process(input);

            require(
                incident.type ==
                    caesar::carr::IncidentType::UNKNOWN,
                "unknown diagnosis failed");

            require(
                has_action(
                    incident,
                    caesar::carr::ActionType::VERIFY_CHAIN),
                "unknown failure verification missing");

            require(
                !has_action(
                    incident,
                    caesar::carr::ActionType::
                        CONSENSUS_CHANGE_REQUIRES_GATE),
                "unknown failure exposed consensus action");
        }

        const auto journal_dir =
            root /
            "doctor-data" /
            "carr" /
            "incidents";

        require(
            std::filesystem::exists(journal_dir),
            "CARR journal directory missing");

        std::size_t journal_count = 0;

        for (const auto& entry :
             std::filesystem::directory_iterator(
                 journal_dir)) {

            if (entry.is_regular_file() &&
                entry.path().extension() == ".json") {

                ++journal_count;
            }
        }

        require(
            journal_count == 7,
            "unexpected CARR journal count");

        std::cout << "CARR_CORE_TEST=PASS\n";
        std::cout << "CARR_INCIDENTS=" << journal_count << "\n";
        std::cout << "CARR_CONSENSUS_GATE=BLOCKED\n";

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr
            << "CARR_CORE_TEST=FAIL\n"
            << e.what()
            << '\n';

        return 1;
    }
}
