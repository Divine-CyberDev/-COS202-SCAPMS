#include "scapms/utils/CsvFileHandler.h"

#include "scapms/model/EmergencyPatient.h"
#include "scapms/model/FollowUpPatient.h"
#include "scapms/model/GeneralPatient.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <unordered_set>

namespace scapms {
namespace {

AppointmentStatus parseStatus(const std::string& value) {
    if (value == "Pending") return AppointmentStatus::Pending;
    if (value == "Completed") return AppointmentStatus::Completed;
    if (value == "Cancelled") return AppointmentStatus::Cancelled;
    throw std::runtime_error("Unknown appointment status in CSV file.");
}

} // namespace

std::string CsvFileHandler::escape(const std::string& field) {
    if (field.find_first_of(",\"\r\n") == std::string::npos) return field;
    std::string escaped = "\"";
    for (char character : field) {
        if (character == '"') escaped += '"';
        escaped += character;
    }
    escaped += '"';
    return escaped;
}

std::vector<std::string> CsvFileHandler::parseRow(const std::string& row) {
    std::vector<std::string> fields;
    std::string field;
    bool quoted = false;
    for (std::size_t i = 0; i < row.size(); ++i) {
        const char character = row[i];
        if (quoted) {
            if (character == '"' && i + 1 < row.size() && row[i + 1] == '"') {
                field += '"';
                ++i;
            } else if (character == '"') {
                quoted = false;
            } else {
                field += character;
            }
        } else if (character == '"' && field.empty()) {
            quoted = true;
        } else if (character == ',') {
            fields.push_back(std::move(field));
            field.clear();
        } else {
            field += character;
        }
    }
    if (quoted) throw std::runtime_error("CSV file contains an unclosed quote.");
    fields.push_back(std::move(field));
    return fields;
}

void CsvFileHandler::save(const std::string& directory,
                          const ClinicDatabase::PatientList& patients,
                          const std::vector<Appointment>& appointments) {
    const std::filesystem::path folder(directory);
    std::filesystem::create_directories(folder);
    const auto patientPath = folder / "patients.csv";
    const auto appointmentPath = folder / "appointments.csv";
    const auto patientTemp = folder / "patients.csv.tmp";
    const auto appointmentTemp = folder / "appointments.csv.tmp";

    {
        std::ofstream output(patientTemp, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("Could not write patients.csv.");
        output << "id,name,age,contact,category,notes\n";
        for (const auto& patient : patients) {
            output << escape(patient->id()) << ',' << escape(patient->name())
                   << ',' << patient->age() << ',' << escape(patient->contact())
                   << ',' << escape(patient->category()) << ','
                   << escape(patient->notes()) << '\n';
        }
        if (!output) throw std::runtime_error("Failed while writing patients.csv.");
    }
    {
        std::ofstream output(appointmentTemp,
                            std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("Could not write appointments.csv.");
        output << "id,patientId,dateTime,reason,status\n";
        for (const auto& appointment : appointments) {
            output << escape(appointment.id()) << ','
                   << escape(appointment.patientId()) << ','
                   << escape(appointment.dateTime()) << ','
                   << escape(appointment.reason()) << ','
                   << Appointment::statusName(appointment.status()) << '\n';
        }
        if (!output) {
            throw std::runtime_error("Failed while writing appointments.csv.");
        }
    }

    std::error_code error;
    std::filesystem::rename(patientTemp, patientPath, error);
    if (error) {
        std::filesystem::remove(patientPath, error);
        error.clear();
        std::filesystem::rename(patientTemp, patientPath, error);
        if (error) throw std::runtime_error("Could not replace patients.csv.");
    }
    std::filesystem::rename(appointmentTemp, appointmentPath, error);
    if (error) {
        std::filesystem::remove(appointmentPath, error);
        error.clear();
        std::filesystem::rename(appointmentTemp, appointmentPath, error);
        if (error) {
            throw std::runtime_error("Could not replace appointments.csv.");
        }
    }
}

CsvFileHandler::LoadedData CsvFileHandler::load(const std::string& directory) {
    const std::filesystem::path folder(directory);
    std::ifstream patientInput(folder / "patients.csv", std::ios::binary);
    std::ifstream appointmentInput(folder / "appointments.csv", std::ios::binary);
    if (!patientInput || !appointmentInput) {
        throw std::runtime_error(
            "Could not open patients.csv and appointments.csv in the selected folder.");
    }

    std::string row;
    if (!std::getline(patientInput, row)) {
        throw std::runtime_error("patients.csv has an invalid header.");
    }
    if (!row.empty() && row.back() == '\r') row.pop_back();
    if (row != "id,name,age,contact,category,notes") {
        throw std::runtime_error("patients.csv has an invalid header.");
    }
    ClinicDatabase::PatientList patients;
    std::unordered_set<std::string> patientIds;
    while (std::getline(patientInput, row)) {
        if (!row.empty() && row.back() == '\r') row.pop_back();
        if (row.empty()) continue;
        const auto fields = parseRow(row);
        if (fields.size() != 6) {
            throw std::runtime_error("A patient row must contain six fields.");
        }
        if (!patientIds.insert(fields[0]).second) {
            throw std::runtime_error("Patient data contains a duplicate ID.");
        }
        int age = 0;
        try {
            std::size_t used = 0;
            age = std::stoi(fields[2], &used);
            if (used != fields[2].size() || age < 0 || age > 120) {
                throw std::runtime_error("Patient age must be between 0 and 120.");
            }
        } catch (const std::exception&) {
            throw std::runtime_error("Patient data contains an invalid age.");
        }
        std::unique_ptr<PatientRecord> patient;
        if (fields[4] == "General") {
            patient = std::make_unique<GeneralPatient>(
                fields[0], fields[1], age, fields[3], fields[5]);
        } else if (fields[4] == "Emergency") {
            patient = std::make_unique<EmergencyPatient>(
                fields[0], fields[1], age, fields[3], fields[5]);
        } else if (fields[4] == "Follow-Up") {
            patient = std::make_unique<FollowUpPatient>(
                fields[0], fields[1], age, fields[3], fields[5]);
        } else {
            throw std::runtime_error("Patient data contains an unknown category.");
        }
        if (patient->id().empty() || patient->name().empty() ||
            patient->contact().empty()) {
            throw std::runtime_error("Patient data contains an empty required field.");
        }
        patients.push_back(std::move(patient));
    }

    if (!std::getline(appointmentInput, row)) {
        throw std::runtime_error("appointments.csv has an invalid header.");
    }
    if (!row.empty() && row.back() == '\r') row.pop_back();
    if (row != "id,patientId,dateTime,reason,status") {
        throw std::runtime_error("appointments.csv has an invalid header.");
    }
    std::vector<Appointment> appointments;
    std::unordered_set<std::string> appointmentIds;
    while (std::getline(appointmentInput, row)) {
        if (!row.empty() && row.back() == '\r') row.pop_back();
        if (row.empty()) continue;
        const auto fields = parseRow(row);
        if (fields.size() != 5) {
            throw std::runtime_error("An appointment row must contain five fields.");
        }
        if (fields[0].empty() || fields[2].empty() || fields[3].empty() ||
            patientIds.find(fields[1]) == patientIds.end()) {
            throw std::runtime_error(
                "Appointment data references a missing patient or required field.");
        }
        if (!appointmentIds.insert(fields[0]).second) {
            throw std::runtime_error("Appointment data contains a duplicate ID.");
        }
        if (!Appointment::isValidDateTime(fields[2])) {
            throw std::runtime_error(
                "Appointment data contains an invalid date or time.");
        }
        appointments.emplace_back(fields[0], fields[1], fields[2], fields[3],
                                  parseStatus(fields[4]));
        auto found = std::find_if(
            patients.begin(), patients.end(), [&fields](const auto& patient) {
                return patient->id() == fields[1];
            });
        (*found)->scheduleAppointment(fields[0]);
    }
    if (!patientInput.eof() || !appointmentInput.eof()) {
        throw std::runtime_error("A data file could not be read completely.");
    }
    return {std::move(patients), std::move(appointments)};
}

} // namespace scapms
