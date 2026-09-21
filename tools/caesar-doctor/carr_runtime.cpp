#include "carr_runtime.hpp"

#include "carr_policy.hpp"
#include "carr_supervisor.hpp"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <atomic>

namespace caesar::carr {
namespace {

std::string make_execution_id() {
    static std::atomic<std::uint64_t> sequence{0};

    const auto now =
        std::chrono::duration_cast<
            std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();

    const auto seq = ++sequence;

    std::ostringstream out;
    out << "CARR-ACT-" << now << "-" << seq;
    return out.str();
}

std::string escape_json(const std::string& value) {
    std::ostringstream out;

    for (const char c : value) {
        switch (c) {
        case '\\':
            out << "\\\\";
            break;
        case '"':
            out << "\\\"";
            break;
        case '\n':
            out << "\\n";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\t':
            out << "\\t";
            break;
        default:
            out << c;
            break;
        }
    }

    return out.str();
}

std::int64_t unix_millis() {
    const auto now =
        std::chrono::system_clock::now();

    return std::chrono::duration_cast<
        std::chrono::milliseconds>(
        now.time_since_epoch()).count();
}

void write_string(
    std::ostream& out,
    const std::string& value) {

    out << '"' << escape_json(value) << '"';
}

}

RuntimeExecutor::RuntimeExecutor(
    std::filesystem::path root,
    RuntimeLimits limits)
    : root_(std::move(root)),
      limits_(limits) {
}

ActionExecution RuntimeExecutor::execute(
    const IncidentRecord& incident,
    ActionType action,
    const RuntimeCommand& command) {

    ActionExecution execution;
    execution.action = action;
    execution.execution_id = make_execution_id();

    if (!autonomous_allowed(action)) {
        execution.status =
            "POLICY_BLOCKED";
        execution.detail =
            "action is not autonomous";
        append_action_json(
            incident,
            execution);
        return execution;
    }

    if (!policy_allows(
            incident.diagnosis,
            action)) {

        execution.status =
            "DIAGNOSIS_POLICY_BLOCKED";
        execution.detail =
            "action is not allowed for diagnosis";
        append_action_json(
            incident,
            execution);
        return execution;
    }

    execution.allowed = true;

    switch (action) {
    case ActionType::RESTART_NODE:
        execution =
            execute_restart_node(
                incident,
                command);
        break;

    case ActionType::CREATE_REPLAY_CASE:
        execution =
            execute_replay_case(
                incident);
        break;

    default:
        execution.status =
            "NOT_IMPLEMENTED";
        execution.detail =
            "runtime adapter is not implemented for this action";
        break;
    }

    append_action_json(
        incident,
        execution);

    return execution;
}

ActionExecution RuntimeExecutor::execute_restart_node(
    const IncidentRecord& incident,
    const RuntimeCommand& command) {

    (void)incident;

    ActionExecution execution;
    execution.allowed = true;
    execution.action =
        ActionType::RESTART_NODE;
    execution.execution_id =
        make_execution_id();

    RuntimeLimits supervisor_limits = limits_;

    CARRSupervisor supervisor(
        root_,
        supervisor_limits);

    const SupervisorResult result =
        supervisor.run(command);

    execution.executed =
        result.started;

    execution.verified =
        result.recovered;

    execution.execution_id =
        result.execution_id.empty()
            ? execution.execution_id
            : result.execution_id;

    execution.restart_count =
        static_cast<int>(
            result.restart_count);

    execution.signal_number =
        result.last_signal;

    execution.exit_code =
        result.last_exit_code;

    execution.status =
        result.status;

    execution.detail =
        result.detail;

    return execution;
}

ActionExecution RuntimeExecutor::execute_replay_case(
    const IncidentRecord& incident) {

    ActionExecution execution;
    execution.allowed = true;
    execution.executed = true;
    execution.action =
        ActionType::CREATE_REPLAY_CASE;
    execution.execution_id =
        make_execution_id();

    try {
        write_replay_case(
            incident);

        execution.verified = true;
        execution.status =
            "RECORDED";
        execution.detail =
            "replay case created";
    } catch (const std::exception& e) {
        execution.verified = false;
        execution.status =
            "FAILED";
        execution.detail =
            e.what();
    }

    return execution;
}

void RuntimeExecutor::write_replay_case(
    const IncidentRecord& incident) const {

    const auto directory =
        root_ /
        "doctor-data" /
        "carr" /
        "replay";

    std::filesystem::create_directories(
        directory);

    const auto file =
        directory /
        (incident.incident_id + ".json");

    std::ofstream out(file);

    if (!out)
        throw std::runtime_error(
            "failed to create replay case");

    out << "{\n";

    out << "  \"incident_id\": ";
    write_string(
        out,
        incident.incident_id);
    out << ",\n";

    out << "  \"created_unix_ms\": ";
    out << unix_millis();
    out << ",\n";

    out << "  \"type\": ";
    write_string(
        out,
        to_string(incident.type));
    out << ",\n";

    out << "  \"fingerprint\": ";
    write_string(
        out,
        incident.fingerprint);
    out << ",\n";

    out << "  \"diagnosis_code\": ";
    write_string(
        out,
        incident.diagnosis.code);
    out << ",\n";

    out << "  \"root_cause\": ";
    write_string(
        out,
        incident.diagnosis.root_cause);
    out << "\n";

    out << "}\n";

    out.close();

    if (!out)
        throw std::runtime_error(
            "failed while writing replay case");
}

void RuntimeExecutor::append_action_json(
    const IncidentRecord& incident,
    const ActionExecution& execution) const {

    const auto directory =
        root_ /
        "doctor-data" /
        "carr" /
        "actions";

    std::filesystem::create_directories(
        directory);

    const auto file =
        directory /
        (incident.incident_id +
         "-" +
         to_string(execution.action) +
         "-" +
         execution.execution_id +
         ".json");

    std::ofstream out(file);

    if (!out)
        throw std::runtime_error(
            "failed to create CARR action journal");

    out << "{\n";

    out << "  \"incident_id\": ";
    write_string(
        out,
        incident.incident_id);
    out << ",\n";

    out << "  \"execution_id\": ";
    write_string(
        out,
        execution.execution_id);
    out << ",\n";

    out << "  \"action\": ";
    write_string(
        out,
        to_string(execution.action));
    out << ",\n";

    out << "  \"allowed\": ";
    out << (execution.allowed ? "true" : "false");
    out << ",\n";

    out << "  \"executed\": ";
    out << (execution.executed ? "true" : "false");
    out << ",\n";

    out << "  \"verified\": ";
    out << (execution.verified ? "true" : "false");
    out << ",\n";

    out << "  \"status\": ";
    write_string(
        out,
        execution.status);
    out << ",\n";

    out << "  \"detail\": ";
    write_string(
        out,
        execution.detail);
    out << ",\n";

    out << "  \"restart_count\": ";
    out << execution.restart_count;
    out << ",\n";

    out << "  \"signal_number\": ";
    out << execution.signal_number;
    out << ",\n";

    out << "  \"exit_code\": ";
    out << execution.exit_code;
    out << "\n";

    out << "}\n";

    out.close();

    if (!out)
        throw std::runtime_error(
            "failed while writing CARR action journal");
}

}
