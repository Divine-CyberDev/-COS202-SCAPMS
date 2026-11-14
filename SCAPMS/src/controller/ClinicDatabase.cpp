#include "scapms/controller/ClinicDatabase.h"

#include <algorithm>

namespace scapms {

const ClinicDatabase::PatientList& ClinicDatabase::patients() const noexcept {
    return patients_;
}

ClinicDatabase::PatientList& ClinicDatabase::mutablePatients() noexcept {
    return patients_;
}

const std::vector<Appointment>& ClinicDatabase::appointments() const noexcept {
    return appointments_;
}

std::vector<Appointment>& ClinicDatabase::mutableAppointments() noexcept {
    return appointments_;
}

std::queue<std::string>& ClinicDatabase::waitingQueue() noexcept {
    return waitingQueue_;
}

const std::queue<std::string>& ClinicDatabase::waitingQueue() const noexcept {
    return waitingQueue_;
}

PatientRecord* ClinicDatabase::findPatient(const std::string& id) noexcept {
    for (auto& patient : patients_) {
        if (patient->id() == id) return patient.get();
    }
    return nullptr;
}

const PatientRecord* ClinicDatabase::findPatient(
    const std::string& id) const noexcept {
    for (const auto& patient : patients_) {
        if (patient->id() == id) return patient.get();
    }
    return nullptr;
}

void ClinicDatabase::rememberPatient(const std::string& id) const noexcept {
    const auto found = std::find(recentPatientIds_.begin(),
                                 recentPatientIds_.end(), id);
    if (found != recentPatientIds_.end()) {
        std::rotate(recentPatientIds_.begin(), found, found + 1);
        return;
    }
    std::move_backward(recentPatientIds_.begin(), recentPatientIds_.end() - 1,
                       recentPatientIds_.end());
    recentPatientIds_.front() = id;
}

const std::array<std::string, 10>&
ClinicDatabase::recentPatientIds() const noexcept {
    return recentPatientIds_;
}

ClinicDatabase::Snapshot ClinicDatabase::makeSnapshot() const {
    Snapshot snapshot;
    snapshot.appointments = appointments_;
    snapshot.waitingQueue = waitingQueue_;
    snapshot.recentPatientIds = recentPatientIds_;
    snapshot.patients.reserve(patients_.size());
    for (const auto& patient : patients_) {
        snapshot.patients.push_back(patient->clone());
    }
    return snapshot;
}

void ClinicDatabase::recordUndoPoint() {
    undoStack_.push(makeSnapshot());
}

bool ClinicDatabase::undo() {
    if (undoStack_.empty()) return false;
    Snapshot previous = std::move(undoStack_.top());
    undoStack_.pop();
    patients_ = std::move(previous.patients);
    appointments_ = std::move(previous.appointments);
    waitingQueue_ = std::move(previous.waitingQueue);
    recentPatientIds_ = std::move(previous.recentPatientIds);
    return true;
}

bool ClinicDatabase::canUndo() const noexcept { return !undoStack_.empty(); }

void ClinicDatabase::clearData() {
    patients_.clear();
    appointments_.clear();
    waitingQueue_ = {};
    recentPatientIds_.fill({});
}

void ClinicDatabase::replaceData(PatientList patients,
                                 std::vector<Appointment> appointments) {
    patients_ = std::move(patients);
    appointments_ = std::move(appointments);
    waitingQueue_ = {};
    recentPatientIds_.fill({});
    undoStack_ = {};
}

} // namespace scapms
