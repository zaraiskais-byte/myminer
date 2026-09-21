#include "carr_journal.hpp"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace caesar::carr {
namespace {

std::string escape_json(const std::string& value) {
    std::ostringstream out;

    for (const char c : value) {
        switch (c) {
        case '\\': out << "\\\\"; break;
        case '"': out << "\\\""; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            out << c;
            break;
        }
    }

    return out.str();
}

void write_string(
    std::ostream& out,
    const std::string& value) {

    out << '"' << escape_json(value) << '"';
}

}

std::string make_incident_id() {
    static std::uint64_t sequence = 0;

    const auto now =
        std::chrono::system_clock::now();

    const auto millis =
        std::chrono::duration_cast<
            std::chrono::milliseconds>(
            now.time_since_epoch()).count();

    ++sequence;

    std::ostringstream out;
    out << "CARR-";
    out << millis;
    out << "-";
    out << sequence;

    return out.str();
}

void append_incident_json(
    const IncidentRecord& record,
    const std::filesystem::path& root) {

    const auto directory =
        root / "doctor-data" / "carr" / "incidents";

    std::filesystem::create_directories(directory);

    const auto file =
        directory /
        (record.incident_id + ".json");

    std::ofstream out(file);

    if (!out)
        throw std::runtime_error(
            "failed to create CARR incident journal");

    out << "{\n";

    out << "  \"incident_id\": ";
    write_string(out, record.incident_id);
    out << ",\n";

    out << "  \"created_unix_ms\": ";
    out << record.created_unix_ms;
    out << ",\n";

    out << "  \"type\": ";
    write_string(out, to_string(record.type));
    out << ",\n";

    out << "  \"state\": ";
    write_string(out, to_string(record.state));
    out << ",\n";

    out << "  \"severity\": ";
    write_string(out, to_string(record.severity));
    out << ",\n";

    out << "  \"fingerprint\": ";
    write_string(out, record.fingerprint);
    out << ",\n";

    out << "  \"diagnosis\": {\n";

    out << "    \"type\": ";
    write_string(out, to_string(record.diagnosis.type));
    out << ",\n";

    out << "    \"code\": ";
    write_string(out, record.diagnosis.code);
    out << ",\n";

    out << "    \"root_cause\": ";
    write_string(out, record.diagnosis.root_cause);
    out << ",\n";

    out << "    \"confidence\": ";
    out << std::fixed << std::setprecision(3);
    out << record.diagnosis.confidence;
    out << ",\n";

    out << "    \"evidence\": [";

    for (std::size_t i = 0;
         i < record.diagnosis.evidence.size();
         ++i) {

        if (i != 0)
            out << ", ";

        write_string(
            out,
            record.diagnosis.evidence[i]);
    }

    out << "]\n";
    out << "  },\n";

    out << "  \"candidates\": [\n";

    for (std::size_t i = 0;
         i < record.candidates.size();
         ++i) {

        const auto& candidate =
            record.candidates[i];

        out << "    {\n";

        out << "      \"action\": ";
        write_string(
            out,
            to_string(candidate.action));
        out << ",\n";

        out << "      \"risk\": ";
        out << candidate.risk;
        out << ",\n";

        out << "      \"autonomous\": ";
        out << (candidate.autonomous ? "true" : "false");
        out << ",\n";

        out << "      \"rationale\": ";
        write_string(
            out,
            candidate.rationale);

        out << "\n";
        out << "    }";

        if (i + 1 != record.candidates.size())
            out << ",";

        out << "\n";
    }

    out << "  ],\n";

    out << "  \"verification\": [\n";

    for (std::size_t i = 0;
         i < record.verification.size();
         ++i) {

        const auto& step =
            record.verification[i];

        out << "    {\n";

        out << "      \"code\": ";
        write_string(out, step.code);
        out << ",\n";

        out << "      \"description\": ";
        write_string(out, step.description);
        out << "\n";

        out << "    }";

        if (i + 1 != record.verification.size())
            out << ",";

        out << "\n";
    }

    out << "  ]\n";
    out << "}\n";

    out.close();

    if (!out)
        throw std::runtime_error(
            "failed while writing CARR incident journal");
}

}
