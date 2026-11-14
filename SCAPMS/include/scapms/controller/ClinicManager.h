#pragma once

#include "scapms/controller/ClinicDatabase.h"
#include "scapms/controller/SearchEngine.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace scapms {

struct ClinicReport {
    std::vector<std::pair<std::string, std::size_t>> mostAttended;
    std::vector<const PatientRecord*> pendingPatients;
    std::size_t generalPatients = 0;
    std::size_t emergencyPatients = 0;
    std::size_t followUpPatients = 0;
    std::size_t appointmentsToday = 0;
    std::size_t completedToday = 0;
    std::size_t pendingToday = 0;
    std::size_t cancelledToday = 0;
};

class ClinicManager {
public:
    const ClinicDatabase::PatientList& patients() const noexcept;
    const std::vector<Appointment>& appointments() const noexcept;
    const ClinicDatabase& database() const noexcept;
    bool canUndo() const noexcept;

    const PatientRecord* addPatient(std::string name, int age,
                                    std::string contact, std::string category,
                                    std::string notes);
    void editPatient(const std::string& id, std::string name, int age,
                     std::string contact, std::string category,
                     std::string notes);
    void deletePatient(const std::string& id);
    const PatientRecord* findPatient(const std::string& id);
    SearchEngine::PatientView searchId(const std::string& id,
                                       bool binary = false) const;
    SearchEngine::PatientView searchName(const std::string& query) const;
    SearchEngine::PatientView searchCategory(const std::string& category) const;
    SearchEngine::PatientView sortedPatients(SortAlgorithm algorithm,
                                             SortField field) const;

    std::string scheduleAppointment(const std::string& patientId,
                                    std::string dateTime, std::string reason);
    void cancelAppointment(const std::string& appointmentId);
    void completeAppointment(const std::string& appointmentId);
    void addToWaitingQueue(const std::string& patientId);
    std::string callNextPatient();
    std::vector<std::string> waitingPatientIds() const;
    bool undoLastOperation();
    void newDataset();

    ClinicReport report(const std::string& date) const;
    void save(const std::string& directory) const;
    void load(const std::string& directory);

private:
    std::string nextPatientId();
    std::string nextAppointmentId();
    void validatePatient(const std::string& name, int age,
                         const std::string& contact) const;

    ClinicDatabase database_;
    unsigned long nextPatientNumber_ = 1;
    unsigned long nextAppointmentNumber_ = 1;
};

} // namespace scapms
