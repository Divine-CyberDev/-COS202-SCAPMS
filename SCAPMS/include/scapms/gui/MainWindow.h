#pragma once

#include "scapms/controller/ClinicManager.h"

#include <QMainWindow>

#include <string>

class QComboBox;
class QDateTimeEdit;
class QLabel;
class QLineEdit;
class QSpinBox;
class QTableWidget;
class QTimer;

namespace scapms {

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    void loadDataFromDirectory(const std::string& directory);

private:
    void buildUi();
    void buildMenus();
    void refreshAll();
    void refreshPatients(const SearchEngine::PatientView* patients = nullptr);
    void refreshAppointments();
    void refreshQueue();
    void refreshDashboard();
    void refreshReports();
    void refreshPatientChoices();
    void addPatient();
    void updatePatient();
    void deleteSelectedPatient();
    void scheduleAppointment();
    void cancelSelectedAppointment();
    void completeSelectedAppointment();
    void addPatientToQueue();
    void callNextPatient();
    void searchPatients();
    void sortPatients();
    void saveData();
    void loadData();
    void startNewDataset();
    void undoLastOperation();
    std::string selectedPatientId() const;
    std::string selectedAppointmentId() const;
    void showError(const std::exception& error);

    ClinicManager manager_;
    QTableWidget* patientTable_ = nullptr;
    QTableWidget* appointmentTable_ = nullptr;
    QTableWidget* queueTable_ = nullptr;
    QTableWidget* searchTable_ = nullptr;
    QTableWidget* frequentTable_ = nullptr;
    QTableWidget* pendingTable_ = nullptr;
    QLineEdit* nameInput_ = nullptr;
    QSpinBox* ageInput_ = nullptr;
    QLineEdit* contactInput_ = nullptr;
    QComboBox* categoryInput_ = nullptr;
    QLineEdit* notesInput_ = nullptr;
    QComboBox* appointmentPatientInput_ = nullptr;
    QDateTimeEdit* appointmentDateInput_ = nullptr;
    QLineEdit* appointmentReasonInput_ = nullptr;
    QComboBox* queuePatientInput_ = nullptr;
    QLineEdit* searchInput_ = nullptr;
    QComboBox* searchFieldInput_ = nullptr;
    QComboBox* searchAlgorithmInput_ = nullptr;
    QComboBox* sortAlgorithmInput_ = nullptr;
    QComboBox* sortFieldInput_ = nullptr;
    QLabel* patientCountLabel_ = nullptr;
    QLabel* appointmentCountLabel_ = nullptr;
    QLabel* waitingCountLabel_ = nullptr;
    QLabel* categorySummaryLabel_ = nullptr;
    QLabel* dailySummaryLabel_ = nullptr;
    QTimer* refreshTimer_ = nullptr;
    std::string lastReminderAppointmentId_;
};

} // namespace scapms
