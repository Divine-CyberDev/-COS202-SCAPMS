#pragma once

#include "scapms/model/Appointment.h"
#include "scapms/model/PatientRecord.h"

#include <array>
#include <memory>
#include <queue>
#include <stack>
#include <string>
#include <vector>

namespace scapms {

class ClinicDatabase {
public:
    using PatientList = std::vector<std::unique_ptr<PatientRecord>>;

    struct Snapshot {
        PatientList patients;
        std::vector<Appointment> appointments;
        std::queue<std::string> waitingQueue;
        std::array<std::string, 10> recentPatientIds{};
    };

    const PatientList& patients() const noexcept;
    PatientList& mutablePatients() noexcept;
    const std::vector<Appointment>& appointments() const noexcept;
    std::vector<Appointment>& mutableAppointments() noexcept;
    std::queue<std::string>& waitingQueue() noexcept;
    const std::queue<std::string>& waitingQueue() const noexcept;

    PatientRecord* findPatient(const std::string& id) noexcept;
    const PatientRecord* findPatient(const std::string& id) const noexcept;
    void rememberPatient(const std::string& id) const noexcept;
    const std::array<std::string, 10>& recentPatientIds() const noexcept;

    void recordUndoPoint();
    bool undo();
    bool canUndo() const noexcept;
    void clearData();
    void replaceData(PatientList patients, std::vector<Appointment> appointments);

private:
    Snapshot makeSnapshot() const;
    PatientList patients_;
    std::vector<Appointment> appointments_;
    std::queue<std::string> waitingQueue_;
    mutable std::array<std::string, 10> recentPatientIds_{};
    std::stack<Snapshot> undoStack_;
};

} // namespace scapms
