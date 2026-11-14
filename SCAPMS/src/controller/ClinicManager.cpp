#include "scapms/controller/ClinicManager.h"

#include "scapms/model/EmergencyPatient.h"
#include "scapms/model/FollowUpPatient.h"
#include "scapms/model/GeneralPatient.h"
#include "scapms/utils/CsvFileHandler.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace scapms {
namespace {

bool validCategory(const std::string& category) {
    return category == "General" || category == "Emergency" ||
           category == "Follow-Up";
}

std::unique_ptr<PatientRecord> createPatient(
    const std::string& id, std::string name, int age, std::string contact,
    const std::string& category, std::string notes) {
    if (category == "Emergency") {
        return std::make_unique<EmergencyPatient>(
            id, std::move(name), age, std::move(contact), std::move(notes));
    }
    if (category == "Follow-Up") {
        return std::make_unique<FollowUpPatient>(
            id, std::move(name), age, std::move(contact), std::move(notes));
    }
    return std::make_unique<GeneralPatient>(
        id, std::move(name), age, std::move(contact), std::move(notes));
}

} // namespace

const ClinicDatabase::PatientList& ClinicManager::patients() const noexcept {
    return database_.patients();
}

const std::vector<Appointment>& ClinicManager::appointments() const noexcept {
    return database_.appointments();
}

const ClinicDatabase& ClinicManager::database() const noexcept {
    return database_;
}

bool ClinicManager::canUndo() const noexcept { return database_.canUndo(); }

void ClinicManager::validatePatient(const std::string& name, int age,
                                    const std::string& contact) const {
    if (name.empty()) throw std::invalid_argument("Patient name is required.");
    if (age < 0 || age > 120) {
        throw std::invalid_argument("Patient age must be between 0 and 120.");
    }
    if (contact.empty()) {
        throw std::invalid_argument("Contact information is required.");
    }
}

std::string ClinicManager::nextPatientId() {
    std::string id;
    do {
        id = "P" + std::to_string(nextPatientNumber_++);
        while (id.size() < 6) id.insert(1, "0");
    } while (database_.findPatient(id) != nullptr);
    return id;
}

std::string ClinicManager::nextAppointmentId() {
    std::string id;
    const auto exists = [this](const std::string& candidate) {
        for (const auto& appointment : database_.appointments()) {
            if (appointment.id() == candidate) return true;
        }
        return false;
    };
    do {
        id = "A" + std::to_string(nextAppointmentNumber_++);
        while (id.size() < 6) id.insert(1, "0");
    } while (exists(id));
    return id;
}

const PatientRecord* ClinicManager::addPatient(std::string name, int age,
                                                std::string contact,
                                                std::string category,
                                                std::string notes) {
    validatePatient(name, age, contact);
    if (!validCategory(category)) {
        throw std::invalid_argument("Select a valid patient category.");
    }

    const std::string id = nextPatientId();
    std::unique_ptr<PatientRecord> patient =
        createPatient(id, std::move(name), age, std::move(contact), category,
                      std::move(notes));
    database_.recordUndoPoint();
    database_.mutablePatients().push_back(std::move(patient));
    return database_.findPatient(id);
}

void ClinicManager::editPatient(const std::string& id, std::string name,
                                int age, std::string contact,
                                std::string category, std::string notes) {
    validatePatient(name, age, contact);
    if (!validCategory(category)) {
        throw std::invalid_argument("Select a valid patient category.");
    }
    PatientRecord* patient = database_.findPatient(id);
    if (patient == nullptr) throw std::out_of_range("Patient was not found.");
    if (patient->category() != category) {
        auto replacement = createPatient(id, std::move(name), age,
                                         std::move(contact), category,
                                         std::move(notes));
        replacement->setAppointmentIds(patient->appointmentIds());
        database_.recordUndoPoint();
        auto& patients = database_.mutablePatients();
        for (auto& current : patients) {
            if (current->id() == id) {
                current = std::move(replacement);
                return;
            }
        }
        throw std::out_of_range("Patient was not found.");
    }
    database_.recordUndoPoint();
    patient->update(std::move(name), age, std::move(contact), std::move(notes));
}

void ClinicManager::deletePatient(const std::string& id) {
    if (database_.findPatient(id) == nullptr) {
        throw std::out_of_range("Patient was not found.");
    }
    database_.recordUndoPoint();

    auto& patients = database_.mutablePatients();
    patients.erase(std::remove_if(patients.begin(), patients.end(),
                                  [&id](const auto& patient) {
                                      return patient->id() == id;
                                  }),
                   patients.end());

    auto& appointments = database_.mutableAppointments();
    appointments.erase(
        std::remove_if(appointments.begin(), appointments.end(),
                       [&id](const Appointment& appointment) {
                           return appointment.patientId() == id;
                       }),
        appointments.end());

    std::queue<std::string> retained;
    auto waiting = database_.waitingQueue();
    while (!waiting.empty()) {
        if (waiting.front() != id) retained.push(waiting.front());
        waiting.pop();
    }
    database_.waitingQueue() = std::move(retained);
}

const PatientRecord* ClinicManager::findPatient(const std::string& id) {
    PatientRecord* patient = database_.findPatient(id);
    if (patient != nullptr) database_.rememberPatient(id);
    return patient;
}

SearchEngine::PatientView ClinicManager::searchId(const std::string& id,
                                                   bool binary) const {
    SearchEngine::PatientView view;
    view.reserve(database_.patients().size());
    for (const auto& patient : database_.patients()) view.push_back(patient.get());
    if (binary) {
        SearchEngine::sort(view, SortAlgorithm::Insertion, SortField::PatientId);
        const auto* match = SearchEngine::binarySearchById(view, id);
        if (match != nullptr) database_.rememberPatient(match->id());
        return match == nullptr ? SearchEngine::PatientView{}
                                : SearchEngine::PatientView{match};
    }
    const auto* match = SearchEngine::linearSearchById(view, id);
    if (match != nullptr) database_.rememberPatient(match->id());
    return match == nullptr ? SearchEngine::PatientView{}
                            : SearchEngine::PatientView{match};
}

SearchEngine::PatientView ClinicManager::searchName(
    const std::string& query) const {
    SearchEngine::PatientView view;
    view.reserve(database_.patients().size());
    for (const auto& patient : database_.patients()) view.push_back(patient.get());
    auto matches = SearchEngine::recursiveSearchByName(view, query);
    for (const PatientRecord* patient : matches) {
        database_.rememberPatient(patient->id());
    }
    return matches;
}

SearchEngine::PatientView ClinicManager::searchCategory(
    const std::string& category) const {
    SearchEngine::PatientView matches;
    for (const auto& patient : database_.patients()) {
        if (category == "All" || patient->category() == category) {
            matches.push_back(patient.get());
            database_.rememberPatient(patient->id());
        }
    }
    return matches;
}

SearchEngine::PatientView ClinicManager::sortedPatients(
    SortAlgorithm algorithm, SortField field) const {
    SearchEngine::PatientView view;
    view.reserve(database_.patients().size());
    for (const auto& patient : database_.patients()) view.push_back(patient.get());
    SearchEngine::sort(view, algorithm, field);
    return view;
}

std::string ClinicManager::scheduleAppointment(const std::string& patientId,
                                                std::string dateTime,
                                                std::string reason) {
    PatientRecord* patient = database_.findPatient(patientId);
    if (patient == nullptr) throw std::out_of_range("Patient was not found.");
    if (!Appointment::isValidDateTime(dateTime) || reason.empty()) {
        throw std::invalid_argument(
            "Enter a valid appointment date and time and a reason.");
    }
    const std::string id = nextAppointmentId();
    database_.recordUndoPoint();
    database_.mutableAppointments().emplace_back(
        id, patientId, std::move(dateTime), std::move(reason));
    patient->scheduleAppointment(id);
    return id;
}

void ClinicManager::cancelAppointment(const std::string& appointmentId) {
    auto& appointments = database_.mutableAppointments();
    auto found = std::find_if(appointments.begin(), appointments.end(),
                              [&appointmentId](const Appointment& appointment) {
                                  return appointment.id() == appointmentId;
                              });
    if (found == appointments.end()) {
        throw std::out_of_range("Appointment was not found.");
    }
    if (found->status() == AppointmentStatus::Cancelled) {
        throw std::invalid_argument("Appointment is already cancelled.");
    }
    database_.recordUndoPoint();
    found = std::find_if(appointments.begin(), appointments.end(),
                         [&appointmentId](const Appointment& appointment) {
                             return appointment.id() == appointmentId;
                         });
    found->setStatus(AppointmentStatus::Cancelled);
}

void ClinicManager::completeAppointment(const std::string& appointmentId) {
    auto& appointments = database_.mutableAppointments();
    auto found = std::find_if(appointments.begin(), appointments.end(),
                              [&appointmentId](const Appointment& appointment) {
                                  return appointment.id() == appointmentId;
                              });
    if (found == appointments.end()) {
        throw std::out_of_range("Appointment was not found.");
    }
    if (found->status() != AppointmentStatus::Pending) {
        throw std::invalid_argument("Only pending appointments can be completed.");
    }
    database_.recordUndoPoint();
    found = std::find_if(appointments.begin(), appointments.end(),
                         [&appointmentId](const Appointment& appointment) {
                             return appointment.id() == appointmentId;
                         });
    found->setStatus(AppointmentStatus::Completed);
}

void ClinicManager::addToWaitingQueue(const std::string& patientId) {
    if (database_.findPatient(patientId) == nullptr) {
        throw std::out_of_range("Patient was not found.");
    }
    auto waiting = waitingPatientIds();
    if (std::find(waiting.begin(), waiting.end(), patientId) != waiting.end()) {
        throw std::invalid_argument("Patient is already in the waiting queue.");
    }
    database_.waitingQueue().push(patientId);
}

std::string ClinicManager::callNextPatient() {
    auto& waiting = database_.waitingQueue();
    if (waiting.empty()) throw std::out_of_range("The waiting queue is empty.");
    const std::string id = waiting.front();
    waiting.pop();
    database_.rememberPatient(id);
    return id;
}

std::vector<std::string> ClinicManager::waitingPatientIds() const {
    std::vector<std::string> ids;
    auto waiting = database_.waitingQueue();
    while (!waiting.empty()) {
        ids.push_back(waiting.front());
        waiting.pop();
    }
    return ids;
}

bool ClinicManager::undoLastOperation() { return database_.undo(); }

void ClinicManager::newDataset() {
    database_.recordUndoPoint();
    database_.clearData();
    nextPatientNumber_ = 1;
    nextAppointmentNumber_ = 1;
}

ClinicReport ClinicManager::report(const std::string& date) const {
    ClinicReport result;
    std::unordered_map<std::string, std::size_t> completedCounts;
    std::unordered_set<std::string> pendingIds;
    for (const auto& patient : database_.patients()) {
        if (patient->category() == "General") ++result.generalPatients;
        else if (patient->category() == "Emergency") ++result.emergencyPatients;
        else if (patient->category() == "Follow-Up") ++result.followUpPatients;
    }
    for (const auto& appointment : database_.appointments()) {
        const std::string appointmentDate = appointment.dateTime().substr(0, 10);
        if (appointment.status() == AppointmentStatus::Completed) {
            ++completedCounts[appointment.patientId()];
        }
        if (appointment.status() == AppointmentStatus::Pending) {
            pendingIds.insert(appointment.patientId());
        }
        if (appointmentDate == date) {
            ++result.appointmentsToday;
            if (appointment.status() == AppointmentStatus::Completed) {
                ++result.completedToday;
            } else if (appointment.status() == AppointmentStatus::Pending) {
                ++result.pendingToday;
            } else {
                ++result.cancelledToday;
            }
        }
    }
    for (const auto& idAndCount : completedCounts) {
        result.mostAttended.push_back(idAndCount);
    }
    for (std::size_t i = 0; i < result.mostAttended.size(); ++i) {
        std::size_t best = i;
        for (std::size_t j = i + 1; j < result.mostAttended.size(); ++j) {
            if (result.mostAttended[j].second > result.mostAttended[best].second) {
                best = j;
            }
        }
        if (best != i) std::swap(result.mostAttended[i], result.mostAttended[best]);
    }
    for (const auto& patient : database_.patients()) {
        if (pendingIds.find(patient->id()) != pendingIds.end()) {
            result.pendingPatients.push_back(patient.get());
        }
    }
    return result;
}

void ClinicManager::save(const std::string& directory) const {
    CsvFileHandler::save(directory, database_.patients(),
                         database_.appointments());
}

void ClinicManager::load(const std::string& directory) {
    CsvFileHandler::LoadedData data = CsvFileHandler::load(directory);
    unsigned long patientNumber = 1;
    unsigned long appointmentNumber = 1;
    const auto parseNumber = [](const std::string& id, char prefix) {
        if (id.size() < 2 || id.front() != prefix) {
            throw std::runtime_error("Data contains an invalid generated ID.");
        }
        std::size_t used = 0;
        const unsigned long number = std::stoul(id.substr(1), &used);
        if (used != id.size() - 1 ||
            number == std::numeric_limits<unsigned long>::max()) {
            throw std::runtime_error("Data contains an invalid generated ID.");
        }
        return number;
    };
    for (const auto& patient : data.first) {
        const std::string& id = patient->id();
        patientNumber = std::max(patientNumber, parseNumber(id, 'P') + 1);
    }
    for (const auto& appointment : data.second) {
        const std::string& id = appointment.id();
        appointmentNumber =
            std::max(appointmentNumber, parseNumber(id, 'A') + 1);
    }
    database_.replaceData(std::move(data.first), std::move(data.second));
    nextPatientNumber_ = patientNumber;
    nextAppointmentNumber_ = appointmentNumber;
}

} // namespace scapms
