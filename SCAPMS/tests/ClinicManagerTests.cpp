#include "scapms/controller/ClinicManager.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void testPatientManagementAndUndo() {
    scapms::ClinicManager manager;
    const auto* patient = manager.addPatient(
        "Avery Patient", 30, "test@example.invalid", "General", "initial note");
    const std::string id = patient->id();
    require(manager.patients().size() == 1, "patient should be registered");
    require(patient->category() == "General", "polymorphic category should match");

    manager.editPatient(id, "Avery Updated", 31, "test@example.invalid",
                        "Emergency", "edited");
    require(manager.database().findPatient(id)->name() == "Avery Updated",
            "patient edit should be applied");
    require(manager.database().findPatient(id)->category() == "Emergency",
            "patient category should be editable");
    require(manager.undoLastOperation(), "edit should be undoable");
    require(manager.database().findPatient(id)->name() == "Avery Patient",
            "undo should restore the pre-edit patient");
    require(manager.database().findPatient(id)->category() == "General",
            "undo should restore the original patient subtype");

    manager.deletePatient(id);
    require(manager.patients().empty(), "patient should be deleted");
    require(manager.undoLastOperation(), "delete should be undoable");
    require(manager.database().findPatient(id) != nullptr,
            "undo should restore the deleted patient");

    bool invalidAgeRejected = false;
    try {
        manager.addPatient("Invalid", 121, "test@example.invalid", "General", "");
    } catch (const std::invalid_argument&) {
        invalidAgeRejected = true;
    }
    require(invalidAgeRejected, "age above 120 should be rejected");
}

void testSearchSortAndPolymorphism() {
    scapms::ClinicManager manager;
    const auto* general = manager.addPatient(
        "Mina General", 25, "mina@example.invalid", "General", "");
    const std::string generalId = general->id();
    manager.addPatient("Zed Emergency", 42, "zed@example.invalid", "Emergency", "");
    manager.addPatient("Ari Followup", 60, "ari@example.invalid", "Follow-Up", "");

    const auto linear = manager.searchId(generalId);
    require(linear.size() == 1 && linear.front()->id() == generalId,
            "linear ID search should find a patient");
    const auto binary = manager.searchId(generalId, true);
    require(binary.size() == 1 && binary.front()->id() == generalId,
            "binary ID search should find a patient");
    const auto recursive = manager.searchName("zed");
    require(recursive.size() == 1 &&
                recursive.front()->category() == "Emergency",
            "recursive name search should match case-insensitively");
    const auto category = manager.searchCategory("Follow-Up");
    require(category.size() == 1 && category.front()->category() == "Follow-Up",
            "category filtering should use the polymorphic category");

    const auto byName = manager.sortedPatients(scapms::SortAlgorithm::Selection,
                                               scapms::SortField::Name);
    require(byName.size() == 3 && byName[0]->name() == "Ari Followup",
            "selection sort should order patient names");
    const auto byId = manager.sortedPatients(scapms::SortAlgorithm::Insertion,
                                             scapms::SortField::PatientId);
    require(byId.size() == 3 && byId[0]->id() < byId[1]->id() &&
                byId[1]->id() < byId[2]->id(),
            "insertion sort should order patient IDs");
    const auto& recent = manager.database().recentPatientIds();
    require(std::find(recent.begin(), recent.end(), generalId) != recent.end(),
            "recent patient cache should record access");
}

void testAppointmentsQueueReportsAndPersistence() {
    scapms::ClinicManager manager;
    const auto* first = manager.addPatient(
        "Patient, \"One\"", 27, "first@example.invalid", "Emergency", "");
    const std::string firstId = first->id();
    const auto* second = manager.addPatient(
        "Second Patient", 40, "second@example.invalid", "Follow-Up", "");
    const std::string secondId = second->id();

    const std::string completedId = manager.scheduleAppointment(
        firstId, "2026-11-04 09:00", "Follow-up, review");
    manager.completeAppointment(completedId);
    const std::string pendingId = manager.scheduleAppointment(
        secondId, "2026-11-05 10:30", "Routine review");
    manager.addToWaitingQueue(firstId);
    bool duplicateQueueEntryRejected = false;
    try {
        manager.addToWaitingQueue(firstId);
    } catch (const std::invalid_argument&) {
        duplicateQueueEntryRejected = true;
    }
    require(duplicateQueueEntryRejected,
            "a patient should not be queued twice at the same time");
    manager.addToWaitingQueue(secondId);
    require(manager.callNextPatient() == firstId,
            "waiting queue should serve patients in FIFO order");
    require(manager.waitingPatientIds().size() == 1,
            "calling the next patient should remove one queue entry");

    const auto report = manager.report("2026-11-04");
    require(report.emergencyPatients == 1 && report.followUpPatients == 1,
            "category report should count patient subclasses");
    require(report.appointmentsToday == 1 && report.completedToday == 1,
            "daily report should count completed appointments");
    require(report.pendingPatients.size() == 1 &&
                report.pendingPatients.front()->id() == secondId,
            "pending report should identify patients with pending appointments");
    require(report.mostAttended.size() == 1 &&
                report.mostAttended.front().first == firstId &&
                report.mostAttended.front().second == 1,
            "attendance report should rank completed visits");

    const auto folder = std::filesystem::temp_directory_path() /
                        ("scapms-test-" +
                         std::to_string(std::chrono::steady_clock::now()
                                            .time_since_epoch()
                                            .count()));
    manager.save(folder.string());
    scapms::ClinicManager restored;
    restored.load(folder.string());
    require(restored.patients().size() == 2,
            "load should restore all patient rows");
    require(restored.database().findPatient(firstId)->name() ==
                "Patient, \"One\"",
            "CSV round trip should preserve commas and quotes");
    require(restored.appointments().size() == 2 &&
                restored.appointments()[0].status() ==
                    scapms::AppointmentStatus::Completed,
            "load should restore appointment history and status");
    require(restored.database().findPatient(firstId)->appointmentIds().size() == 1,
            "loaded appointment associations should be restored");

    {
        std::ofstream corrupt(folder / "appointments.csv",
                              std::ios::binary | std::ios::trunc);
        corrupt << "invalid header\n";
    }
    bool invalidLoadRejected = false;
    try {
        restored.load(folder.string());
    } catch (const std::exception&) {
        invalidLoadRejected = true;
    }
    require(invalidLoadRejected && restored.patients().size() == 2 &&
                restored.appointments().size() == 2,
            "a failed load should leave the current clinic data unchanged");

    std::filesystem::remove(folder / "patients.csv");
    std::filesystem::remove(folder / "appointments.csv");
    std::filesystem::remove(folder);

    manager.cancelAppointment(pendingId);
    require(manager.appointments()[1].status() ==
                scapms::AppointmentStatus::Cancelled,
            "cancellation should update the appointment status");
    require(manager.undoLastOperation(), "appointment cancellation should undo");
    require(manager.appointments()[1].status() == scapms::AppointmentStatus::Pending,
            "undo should restore the previous appointment status");
}

void testNewDatasetUndo() {
    scapms::ClinicManager manager;
    manager.addPatient("Temporary Patient", 20, "test@example.invalid",
                       "General", "");
    manager.newDataset();
    require(manager.patients().empty(), "new dataset should clear patient records");
    require(manager.undoLastOperation(), "new dataset should be undoable");
    require(manager.patients().size() == 1,
            "undo should restore data cleared by new dataset");
}

void testBundledDemoData() {
    scapms::ClinicManager manager;
    manager.load(SCAPMS_TEST_DATA_DIR);
    require(manager.patients().size() == 5,
            "bundled demo data should load all fictional patients");
    require(manager.appointments().size() == 5,
            "bundled demo data should load all sample appointments");
    require(manager.report("2026-11-04").appointmentsToday == 3,
            "demo data should exercise daily appointment reports");
}

} // namespace

int main() {
    try {
        testPatientManagementAndUndo();
        testSearchSortAndPolymorphism();
        testAppointmentsQueueReportsAndPersistence();
        testNewDatasetUndo();
        testBundledDemoData();
        std::cout << "All SCAPMS tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "SCAPMS test failure: " << error.what() << '\n';
        return 1;
    }
}
