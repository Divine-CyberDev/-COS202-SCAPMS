#include "scapms/gui/MainWindow.h"

#include "scapms/model/Appointment.h"

#include <QAction>
#include <QAbstractItemView>
#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QDateTimeEdit>
#include <QDialog>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <exception>
#include <filesystem>
#include <utility>

namespace scapms {
namespace {

QString fromUtf8(const std::string& value) {
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

std::string toUtf8(const QString& value) {
    const QByteArray bytes = value.toUtf8();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

QTableWidget* makeTable(const QStringList& headers, QWidget* parent = nullptr) {
    auto* table = new QTableWidget(parent);
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    table->verticalHeader()->setVisible(false);
    return table;
}

void setCell(QTableWidget* table, int row, int column, const QString& text) {
    auto* item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    table->setItem(row, column, item);
}

QString patientDisplay(const PatientRecord* patient) {
    return fromUtf8(patient->id()) + "  |  " + fromUtf8(patient->name());
}

QColor categoryColor(const std::string& category) {
    if (category == "Emergency") return QColor("#fff0ed");
    if (category == "Follow-Up") return QColor("#eef4ff");
    return QColor("#eff8f1");
}

QColor appointmentColor(AppointmentStatus status) {
    if (status == AppointmentStatus::Completed) return QColor("#eff8f1");
    if (status == AppointmentStatus::Cancelled) return QColor("#f2f2f2");
    return QColor("#fff8e6");
}

} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("SCAPMS | Smart Clinic");
    resize(1200, 780);
    buildUi();
    buildMenus();
    refreshAll();

    refreshTimer_ = new QTimer(this);
    refreshTimer_->setInterval(60'000);
    connect(refreshTimer_, &QTimer::timeout, this, [this] {
        refreshDashboard();
        const QDateTime now = QDateTime::currentDateTime();
        for (const auto& appointment : manager_.appointments()) {
            if (appointment.status() != AppointmentStatus::Pending) continue;
            const QDateTime appointmentTime =
                QDateTime::fromString(fromUtf8(appointment.dateTime()),
                                      "yyyy-MM-dd HH:mm");
            const qint64 minutes = now.secsTo(appointmentTime) / 60;
            if (minutes >= 0 && minutes <= 30 &&
                lastReminderAppointmentId_ != appointment.id()) {
                lastReminderAppointmentId_ = appointment.id();
                statusBar()->showMessage(
                    "Appointment reminder: " +
                        fromUtf8(appointment.id()) + " is due within 30 minutes.",
                    12'000);
                break;
            }
        }
    });
    refreshTimer_->start();
}

void MainWindow::buildUi() {
    auto* central = new QWidget(this);
    auto* outer = new QVBoxLayout(central);
    auto* heading = new QLabel("Smart Clinic Appointment & Patient Management");
    QFont headingFont = heading->font();
    headingFont.setPointSize(19);
    headingFont.setBold(true);
    heading->setFont(headingFont);
    heading->setStyleSheet("color: #17324d; padding: 8px 2px;");
    outer->addWidget(heading);

    auto* tabs = new QTabWidget(central);
    outer->addWidget(tabs);

    auto* dashboard = new QWidget;
    auto* dashboardLayout = new QVBoxLayout(dashboard);
    auto* cards = new QGridLayout;
    patientCountLabel_ = new QLabel;
    appointmentCountLabel_ = new QLabel;
    waitingCountLabel_ = new QLabel;
    const auto addCard = [cards](const QString& title, QLabel*& value, int col) {
        auto* card = new QWidget;
        auto* layout = new QVBoxLayout(card);
        auto* titleLabel = new QLabel(title);
        titleLabel->setStyleSheet("color: #546575;");
        value = new QLabel("0");
        QFont valueFont = value->font();
        valueFont.setPointSize(24);
        valueFont.setBold(true);
        value->setFont(valueFont);
        layout->addWidget(titleLabel);
        layout->addWidget(value);
        card->setStyleSheet(
            "background: #f2f6fa; border: 1px solid #dce5ed; border-radius: 8px;");
        cards->addWidget(card, 0, col);
    };
    addCard("Registered patients", patientCountLabel_, 0);
    addCard("Appointments", appointmentCountLabel_, 1);
    addCard("Waiting in queue", waitingCountLabel_, 2);
    dashboardLayout->addLayout(cards);
    auto* overview = new QLabel(
        "Use the tabs to register patients, manage appointments, call the waiting "
        "queue, search and sort records, and review clinic reports. Emergency "
        "patients are highlighted and should receive urgent triage.");
    overview->setWordWrap(true);
    overview->setStyleSheet("padding: 14px; background: #f8fafc; border-radius: 8px;");
    dashboardLayout->addWidget(overview);
    dashboardLayout->addStretch();
    tabs->addTab(dashboard, "Dashboard");

    auto* patientsPage = new QWidget;
    auto* patientsLayout = new QHBoxLayout(patientsPage);
    patientTable_ = makeTable(
        {"ID", "Name", "Age", "Contact", "Category", "Care guidance"});
    patientTable_->setObjectName("patientTable");
    patientsLayout->addWidget(patientTable_, 3);
    auto* patientForm = new QWidget;
    auto* patientFormLayout = new QVBoxLayout(patientForm);
    auto* patientFormTitle = new QLabel("Patient details");
    patientFormTitle->setStyleSheet("font-weight: bold; font-size: 15px;");
    patientFormLayout->addWidget(patientFormTitle);
    auto* patientFields = new QFormLayout;
    nameInput_ = new QLineEdit;
    nameInput_->setMaxLength(100);
    ageInput_ = new QSpinBox;
    ageInput_->setRange(0, 120);
    ageInput_->setValue(18);
    contactInput_ = new QLineEdit;
    contactInput_->setMaxLength(120);
    categoryInput_ = new QComboBox;
    categoryInput_->addItems({"General", "Emergency", "Follow-Up"});
    notesInput_ = new QLineEdit;
    notesInput_->setMaxLength(500);
    notesInput_->setPlaceholderText("Optional consultation notes");
    patientFields->addRow("Name", nameInput_);
    patientFields->addRow("Age", ageInput_);
    patientFields->addRow("Contact", contactInput_);
    patientFields->addRow("Category", categoryInput_);
    patientFields->addRow("Notes", notesInput_);
    patientFormLayout->addLayout(patientFields);
    auto* addButton = new QPushButton("Register patient");
    addButton->setObjectName("registerPatientButton");
    auto* editButton = new QPushButton("Save changes");
    auto* deleteButton = new QPushButton("Delete selected");
    auto* clearButton = new QPushButton("Clear form");
    addButton->setToolTip("Register a new patient (Ctrl+N)");
    editButton->setToolTip("Apply the form values to the selected patient");
    deleteButton->setToolTip("Delete the selected patient and linked records");
    patientFormLayout->addWidget(addButton);
    patientFormLayout->addWidget(editButton);
    patientFormLayout->addWidget(deleteButton);
    patientFormLayout->addWidget(clearButton);
    patientFormLayout->addStretch();
    patientsLayout->addWidget(patientForm, 1);
    tabs->addTab(patientsPage, "Patients");

    auto* appointmentsPage = new QWidget;
    auto* appointmentsLayout = new QVBoxLayout(appointmentsPage);
    auto* appointmentForm = new QFormLayout;
    appointmentPatientInput_ = new QComboBox;
    appointmentDateInput_ = new QDateTimeEdit(QDateTime::currentDateTime());
    appointmentDateInput_->setDisplayFormat("yyyy-MM-dd HH:mm");
    appointmentDateInput_->setCalendarPopup(true);
    appointmentDateInput_->setMinimumDateTime(QDateTime::currentDateTime());
    appointmentReasonInput_ = new QLineEdit;
    appointmentReasonInput_->setMaxLength(200);
    appointmentForm->addRow("Patient", appointmentPatientInput_);
    appointmentForm->addRow("Date and time", appointmentDateInput_);
    appointmentForm->addRow("Reason", appointmentReasonInput_);
    appointmentsLayout->addLayout(appointmentForm);
    auto* appointmentActions = new QHBoxLayout;
    auto* scheduleButton = new QPushButton("Schedule appointment");
    auto* cancelAppointmentButton = new QPushButton("Cancel selected");
    auto* completeAppointmentButton = new QPushButton("Mark completed");
    scheduleButton->setToolTip("Create a pending appointment");
    appointmentActions->addWidget(scheduleButton);
    appointmentActions->addWidget(cancelAppointmentButton);
    appointmentActions->addWidget(completeAppointmentButton);
    appointmentActions->addStretch();
    appointmentsLayout->addLayout(appointmentActions);
    appointmentTable_ = makeTable({"ID", "Patient", "Date / time", "Reason", "Status"});
    appointmentTable_->setObjectName("appointmentTable");
    appointmentsLayout->addWidget(appointmentTable_);
    tabs->addTab(appointmentsPage, "Appointments");

    auto* queuePage = new QWidget;
    auto* queueLayout = new QVBoxLayout(queuePage);
    auto* queueActions = new QHBoxLayout;
    queuePatientInput_ = new QComboBox;
    queuePatientInput_->setObjectName("queuePatientInput");
    auto* enqueueButton = new QPushButton("Add to queue");
    enqueueButton->setObjectName("enqueuePatientButton");
    auto* callNextButton = new QPushButton("Call next patient");
    queueActions->addWidget(new QLabel("Patient"));
    queueActions->addWidget(queuePatientInput_, 1);
    queueActions->addWidget(enqueueButton);
    queueActions->addWidget(callNextButton);
    queueLayout->addLayout(queueActions);
    queueTable_ = makeTable({"Position", "Patient ID", "Patient name", "Category"});
    queueLayout->addWidget(queueTable_);
    tabs->addTab(queuePage, "Waiting queue");

    auto* searchPage = new QWidget;
    auto* searchLayout = new QVBoxLayout(searchPage);
    auto* searchControls = new QGridLayout;
    searchInput_ = new QLineEdit;
    searchInput_->setObjectName("searchInput");
    searchInput_->setPlaceholderText("Enter patient ID, name, or category");
    searchFieldInput_ = new QComboBox;
    searchFieldInput_->setObjectName("searchFieldInput");
    searchFieldInput_->addItems({"Patient ID", "Name", "Category"});
    searchAlgorithmInput_ = new QComboBox;
    searchAlgorithmInput_->setObjectName("searchAlgorithmInput");
    searchAlgorithmInput_->addItems({"Linear search", "Binary search", "Recursive name search"});
    auto* searchButton = new QPushButton("Search");
    searchButton->setObjectName("searchButton");
    searchControls->addWidget(new QLabel("Search for"), 0, 0);
    searchControls->addWidget(searchInput_, 0, 1);
    searchControls->addWidget(searchFieldInput_, 0, 2);
    searchControls->addWidget(searchAlgorithmInput_, 0, 3);
    searchControls->addWidget(searchButton, 0, 4);
    sortAlgorithmInput_ = new QComboBox;
    sortAlgorithmInput_->setObjectName("sortAlgorithmInput");
    sortAlgorithmInput_->addItems({"Selection sort", "Insertion sort"});
    sortFieldInput_ = new QComboBox;
    sortFieldInput_->setObjectName("sortFieldInput");
    sortFieldInput_->addItems({"Patient ID", "Patient name"});
    auto* sortButton = new QPushButton("Sort records");
    sortButton->setObjectName("sortButton");
    searchControls->addWidget(new QLabel("Sort algorithm"), 1, 0);
    searchControls->addWidget(sortAlgorithmInput_, 1, 1);
    searchControls->addWidget(new QLabel("Sort by"), 1, 2);
    searchControls->addWidget(sortFieldInput_, 1, 3);
    searchControls->addWidget(sortButton, 1, 4);
    searchLayout->addLayout(searchControls);
    auto* algorithmNote = new QLabel(
        "Selection sort uses O(n²) comparisons and is easy to trace for small clinic "
        "lists. Insertion sort is also O(n²) worst-case but performs well on small or "
        "nearly sorted lists. Binary search sorts an ID view first; recursive search "
        "matches names.");
    algorithmNote->setWordWrap(true);
    algorithmNote->setStyleSheet("color: #546575; padding: 6px;");
    searchLayout->addWidget(algorithmNote);
    searchTable_ = makeTable(
        {"ID", "Name", "Age", "Contact", "Category", "Care guidance"});
    searchLayout->addWidget(searchTable_);
    tabs->addTab(searchPage, "Search & sort");

    auto* reportsPage = new QWidget;
    auto* reportsLayout = new QVBoxLayout(reportsPage);
    categorySummaryLabel_ = new QLabel;
    dailySummaryLabel_ = new QLabel;
    reportsLayout->addWidget(categorySummaryLabel_);
    reportsLayout->addWidget(dailySummaryLabel_);
    reportsLayout->addWidget(new QLabel("Most frequently attended patients"));
    frequentTable_ = makeTable({"Patient ID", "Name", "Completed visits"});
    frequentTable_->setMaximumHeight(180);
    reportsLayout->addWidget(frequentTable_);
    reportsLayout->addWidget(new QLabel("Patients with pending appointments"));
    pendingTable_ = makeTable({"Patient ID", "Name", "Category"});
    reportsLayout->addWidget(pendingTable_);
    tabs->addTab(reportsPage, "Reports");

    setCentralWidget(central);
    statusBar()->showMessage("Ready");

    connect(addButton, &QPushButton::clicked, this, [this] { addPatient(); });
    connect(editButton, &QPushButton::clicked, this, [this] { updatePatient(); });
    connect(deleteButton, &QPushButton::clicked, this,
            [this] { deleteSelectedPatient(); });
    connect(clearButton, &QPushButton::clicked, this, [this] {
        patientTable_->clearSelection();
        nameInput_->clear();
        contactInput_->clear();
        notesInput_->clear();
        ageInput_->setValue(18);
        categoryInput_->setCurrentIndex(0);
    });
    connect(patientTable_, &QTableWidget::cellClicked, this,
            [this](int row, int) {
                if (!patientTable_->item(row, 0)) return;
                const std::string id = toUtf8(patientTable_->item(row, 0)->text());
                const PatientRecord* patient = manager_.findPatient(id);
                if (patient == nullptr) return;
                nameInput_->setText(fromUtf8(patient->name()));
                ageInput_->setValue(patient->age());
                contactInput_->setText(fromUtf8(patient->contact()));
                categoryInput_->setCurrentText(fromUtf8(patient->category()));
                notesInput_->setText(fromUtf8(patient->notes()));
                refreshDashboard();
            });
    connect(categoryInput_, &QComboBox::currentTextChanged, this,
            [this](const QString& category) {
                if (category == "Emergency") {
                    notesInput_->setPlaceholderText("Triage / emergency notes");
                } else if (category == "Follow-Up") {
                    notesInput_->setPlaceholderText("Previous visit / review notes");
                } else {
                    notesInput_->setPlaceholderText("Optional consultation notes");
                }
            });
    connect(scheduleButton, &QPushButton::clicked, this,
            [this] { scheduleAppointment(); });
    connect(cancelAppointmentButton, &QPushButton::clicked, this,
            [this] { cancelSelectedAppointment(); });
    connect(completeAppointmentButton, &QPushButton::clicked, this,
            [this] { completeSelectedAppointment(); });
    connect(enqueueButton, &QPushButton::clicked, this,
            [this] { addPatientToQueue(); });
    connect(callNextButton, &QPushButton::clicked, this,
            [this] { callNextPatient(); });
    connect(searchButton, &QPushButton::clicked, this,
            [this] { searchPatients(); });
    connect(searchFieldInput_, &QComboBox::currentTextChanged, this,
            [this](const QString& field) {
                searchAlgorithmInput_->clear();
                if (field == "Patient ID") {
                    searchAlgorithmInput_->addItems(
                        {"Linear search", "Binary search"});
                } else if (field == "Name") {
                    searchAlgorithmInput_->addItems(
                        {"Linear search", "Recursive name search"});
                } else {
                    searchAlgorithmInput_->addItem("Linear search");
                }
            });
    connect(searchInput_, &QLineEdit::returnPressed, this,
            [this] { searchPatients(); });
    connect(sortButton, &QPushButton::clicked, this,
            [this] { sortPatients(); });
}

void MainWindow::buildMenus() {
    auto* fileMenu = menuBar()->addMenu("&File");
    auto* newAction = fileMenu->addAction("&New dataset");
    newAction->setShortcut(QKeySequence::New);
    auto* saveAction = fileMenu->addAction("&Save data...");
    saveAction->setShortcut(QKeySequence::Save);
    auto* loadAction = fileMenu->addAction("&Load data...");
    loadAction->setShortcut(QKeySequence::Open);
    fileMenu->addSeparator();
    auto* exitAction = fileMenu->addAction("E&xit");
    exitAction->setShortcut(QKeySequence::Quit);

    auto* adminMenu = menuBar()->addMenu("&Administration");
    auto* undoAction = adminMenu->addAction("&Undo last operation");
    undoAction->setShortcut(QKeySequence("Ctrl+Z"));

    connect(newAction, &QAction::triggered, this,
            [this] { startNewDataset(); });
    connect(saveAction, &QAction::triggered, this, [this] { saveData(); });
    connect(loadAction, &QAction::triggered, this, [this] { loadData(); });
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
    connect(undoAction, &QAction::triggered, this,
            [this] { undoLastOperation(); });

    auto* findShortcut = new QShortcut(QKeySequence("Ctrl+F"), this);
    connect(findShortcut, &QShortcut::activated, this, [this] {
        for (int i = 0; i < centralWidget()->findChild<QTabWidget*>()->count(); ++i) {
            if (centralWidget()->findChild<QTabWidget*>()->tabText(i) ==
                "Search & sort") {
                centralWidget()->findChild<QTabWidget*>()->setCurrentIndex(i);
                searchInput_->setFocus();
                break;
            }
        }
    });
    auto* registerShortcut = new QShortcut(QKeySequence("Ctrl+Shift+N"), this);
    connect(registerShortcut, &QShortcut::activated, this,
            [this] { nameInput_->setFocus(); });
}

void MainWindow::refreshAll() {
    refreshPatients();
    refreshAppointments();
    refreshQueue();
    refreshPatientChoices();
    refreshDashboard();
    refreshReports();
}

void MainWindow::refreshPatients(const SearchEngine::PatientView* patients) {
    SearchEngine::PatientView current;
    if (patients == nullptr) {
        current.reserve(manager_.patients().size());
        for (const auto& patient : manager_.patients()) {
            current.push_back(patient.get());
        }
        patients = &current;
    }
    patientTable_->setRowCount(static_cast<int>(patients->size()));
    for (std::size_t i = 0; i < patients->size(); ++i) {
        const PatientRecord* patient = (*patients)[i];
        const int row = static_cast<int>(i);
        setCell(patientTable_, row, 0, fromUtf8(patient->id()));
        setCell(patientTable_, row, 1, fromUtf8(patient->name()));
        setCell(patientTable_, row, 2, QString::number(patient->age()));
        setCell(patientTable_, row, 3, fromUtf8(patient->contact()));
        setCell(patientTable_, row, 4, fromUtf8(patient->category()));
        setCell(patientTable_, row, 5, fromUtf8(patient->careGuidance()));
        for (int column = 0; column < patientTable_->columnCount(); ++column) {
            patientTable_->item(row, column)->setBackground(
                categoryColor(patient->category()));
        }
    }
}

void MainWindow::refreshAppointments() {
    const auto& appointments = manager_.appointments();
    appointmentTable_->setRowCount(static_cast<int>(appointments.size()));
    for (std::size_t i = 0; i < appointments.size(); ++i) {
        const Appointment& appointment = appointments[i];
        const int row = static_cast<int>(i);
        const PatientRecord* patient =
            manager_.database().findPatient(appointment.patientId());
        setCell(appointmentTable_, row, 0, fromUtf8(appointment.id()));
        setCell(appointmentTable_, row, 1,
                patient == nullptr ? fromUtf8(appointment.patientId())
                                   : fromUtf8(patient->name()));
        setCell(appointmentTable_, row, 2, fromUtf8(appointment.dateTime()));
        setCell(appointmentTable_, row, 3, fromUtf8(appointment.reason()));
        setCell(appointmentTable_, row, 4,
                fromUtf8(Appointment::statusName(appointment.status())));
        for (int column = 0; column < appointmentTable_->columnCount(); ++column) {
            appointmentTable_->item(row, column)->setBackground(
                appointmentColor(appointment.status()));
        }
    }
}

void MainWindow::refreshQueue() {
    const auto ids = manager_.waitingPatientIds();
    queueTable_->setRowCount(static_cast<int>(ids.size()));
    for (std::size_t i = 0; i < ids.size(); ++i) {
        const PatientRecord* patient = manager_.database().findPatient(ids[i]);
        const int row = static_cast<int>(i);
        setCell(queueTable_, row, 0, QString::number(i + 1));
        setCell(queueTable_, row, 1, fromUtf8(ids[i]));
        setCell(queueTable_, row, 2,
                patient == nullptr ? QString("Unknown") : fromUtf8(patient->name()));
        setCell(queueTable_, row, 3,
                patient == nullptr ? QString() : fromUtf8(patient->category()));
    }
}

void MainWindow::refreshDashboard() {
    patientCountLabel_->setText(QString::number(manager_.patients().size()));
    appointmentCountLabel_->setText(
        QString::number(manager_.appointments().size()));
    waitingCountLabel_->setText(
        QString::number(manager_.waitingPatientIds().size()));
}

void MainWindow::refreshReports() {
    const std::string today = toUtf8(QDate::currentDate().toString("yyyy-MM-dd"));
    const ClinicReport report = manager_.report(today);
    categorySummaryLabel_->setText(
        QString("Patient category distribution  |  General: %1    Emergency: %2"
                "    Follow-Up: %3")
            .arg(report.generalPatients)
            .arg(report.emergencyPatients)
            .arg(report.followUpPatients));
    dailySummaryLabel_->setText(
        QString("Today's appointments (%1)  |  Completed: %2    Pending: %3"
                "    Cancelled: %4")
            .arg(report.appointmentsToday)
            .arg(report.completedToday)
            .arg(report.pendingToday)
            .arg(report.cancelledToday));

    frequentTable_->setRowCount(static_cast<int>(report.mostAttended.size()));
    for (std::size_t i = 0; i < report.mostAttended.size(); ++i) {
        const auto& [id, count] = report.mostAttended[i];
        const PatientRecord* patient = manager_.database().findPatient(id);
        const int row = static_cast<int>(i);
        setCell(frequentTable_, row, 0, fromUtf8(id));
        setCell(frequentTable_, row, 1,
                patient == nullptr ? QString() : fromUtf8(patient->name()));
        setCell(frequentTable_, row, 2, QString::number(count));
    }
    pendingTable_->setRowCount(
        static_cast<int>(report.pendingPatients.size()));
    for (std::size_t i = 0; i < report.pendingPatients.size(); ++i) {
        const PatientRecord* patient = report.pendingPatients[i];
        const int row = static_cast<int>(i);
        setCell(pendingTable_, row, 0, fromUtf8(patient->id()));
        setCell(pendingTable_, row, 1, fromUtf8(patient->name()));
        setCell(pendingTable_, row, 2, fromUtf8(patient->category()));
    }
}

void MainWindow::refreshPatientChoices() {
    const auto update = [this](QComboBox* combo) {
        const QString current = combo->currentData().toString();
        combo->clear();
        for (const auto& patient : manager_.patients()) {
            combo->addItem(patientDisplay(patient.get()),
                           fromUtf8(patient->id()));
        }
        const int index = combo->findData(current);
        if (index >= 0) combo->setCurrentIndex(index);
    };
    update(appointmentPatientInput_);
    update(queuePatientInput_);
}

void MainWindow::addPatient() {
    try {
        const PatientRecord* patient = manager_.addPatient(
            toUtf8(nameInput_->text()), ageInput_->value(),
            toUtf8(contactInput_->text()),
            toUtf8(categoryInput_->currentText()), toUtf8(notesInput_->text()));
        refreshAll();
        statusBar()->showMessage("Registered patient " + fromUtf8(patient->id()), 5000);
    } catch (const std::exception& error) {
        showError(error);
    }
}

std::string MainWindow::selectedPatientId() const {
    const auto rows = patientTable_->selectionModel()->selectedRows();
    if (!rows.empty() && patientTable_->item(rows.front().row(), 0)) {
        return toUtf8(patientTable_->item(rows.front().row(), 0)->text());
    }
    return {};
}

void MainWindow::updatePatient() {
    const std::string id = selectedPatientId();
    if (id.empty()) {
        QMessageBox::information(this, "Select a patient",
                                 "Select a patient row before saving changes.");
        return;
    }
    try {
        manager_.editPatient(id, toUtf8(nameInput_->text()), ageInput_->value(),
                             toUtf8(contactInput_->text()),
                             toUtf8(categoryInput_->currentText()),
                             toUtf8(notesInput_->text()));
        refreshAll();
        statusBar()->showMessage("Patient record updated", 5000);
    } catch (const std::exception& error) {
        showError(error);
    }
}

void MainWindow::deleteSelectedPatient() {
    const std::string id = selectedPatientId();
    if (id.empty()) {
        QMessageBox::information(this, "Select a patient",
                                 "Select a patient row before deleting.");
        return;
    }
    if (QMessageBox::question(
            this, "Delete patient",
            "Delete this patient and their linked appointments? This can be undone.") !=
        QMessageBox::Yes) {
        return;
    }
    try {
        manager_.deletePatient(id);
        refreshAll();
        statusBar()->showMessage("Patient and linked records deleted", 5000);
    } catch (const std::exception& error) {
        showError(error);
    }
}

void MainWindow::scheduleAppointment() {
    const std::string patientId =
        toUtf8(appointmentPatientInput_->currentData().toString());
    if (patientId.empty()) {
        QMessageBox::information(this, "No patients",
                                 "Register a patient before scheduling.");
        return;
    }
    try {
        const QDateTime dateTime = appointmentDateInput_->dateTime();
        if (!dateTime.isValid() || dateTime < QDateTime::currentDateTime()) {
            throw std::invalid_argument("Choose a valid future appointment time.");
        }
        const std::string id = manager_.scheduleAppointment(
            patientId, toUtf8(dateTime.toString("yyyy-MM-dd HH:mm")),
            toUtf8(appointmentReasonInput_->text()));
        refreshAll();
        statusBar()->showMessage("Scheduled appointment " + fromUtf8(id), 5000);
    } catch (const std::exception& error) {
        showError(error);
    }
}

std::string MainWindow::selectedAppointmentId() const {
    const auto rows = appointmentTable_->selectionModel()->selectedRows();
    if (rows.empty() || !appointmentTable_->item(rows.front().row(), 0)) return {};
    return toUtf8(appointmentTable_->item(rows.front().row(), 0)->text());
}

void MainWindow::cancelSelectedAppointment() {
    const std::string id = selectedAppointmentId();
    if (id.empty()) {
        QMessageBox::information(this, "Select an appointment",
                                 "Select an appointment row first.");
        return;
    }
    try {
        manager_.cancelAppointment(id);
        refreshAll();
        statusBar()->showMessage("Appointment cancelled", 5000);
    } catch (const std::exception& error) {
        showError(error);
    }
}

void MainWindow::completeSelectedAppointment() {
    const std::string id = selectedAppointmentId();
    if (id.empty()) {
        QMessageBox::information(this, "Select an appointment",
                                 "Select an appointment row first.");
        return;
    }
    try {
        manager_.completeAppointment(id);
        refreshAll();
        statusBar()->showMessage("Appointment marked completed", 5000);
    } catch (const std::exception& error) {
        showError(error);
    }
}

void MainWindow::addPatientToQueue() {
    const std::string id = toUtf8(queuePatientInput_->currentData().toString());
    if (id.empty()) {
        QMessageBox::information(this, "No patients",
                                 "Register a patient before adding to the queue.");
        return;
    }
    try {
        manager_.addToWaitingQueue(id);
        refreshAll();
        statusBar()->showMessage("Patient added to the waiting queue", 5000);
    } catch (const std::exception& error) {
        showError(error);
    }
}

void MainWindow::callNextPatient() {
    try {
        const std::string id = manager_.callNextPatient();
        const PatientRecord* patient = manager_.findPatient(id);
        refreshAll();
        QMessageBox::information(
            this, "Next patient",
            "Please call " +
                (patient == nullptr ? fromUtf8(id) : fromUtf8(patient->name())) +
                " (" + fromUtf8(id) + ").");
    } catch (const std::exception& error) {
        showError(error);
    }
}

void MainWindow::searchPatients() {
    const std::string query = toUtf8(searchInput_->text());
    SearchEngine::PatientView results;
    const QString field = searchFieldInput_->currentText();
    const QString algorithm = searchAlgorithmInput_->currentText();
    try {
        if (field == "Patient ID") {
            if (algorithm == "Binary search") {
                results = manager_.searchId(query, true);
            } else {
                results = manager_.searchId(query, false);
            }
        } else if (field == "Name") {
            if (algorithm == "Recursive name search") {
                results = manager_.searchName(query);
            } else {
                SearchEngine::PatientView all;
                all.reserve(manager_.patients().size());
                for (const auto& patient : manager_.patients()) {
                    all.push_back(patient.get());
                }
                results = SearchEngine::linearSearchByName(all, query);
            }
        } else {
            results = manager_.searchCategory(query);
        }
        searchTable_->setRowCount(static_cast<int>(results.size()));
        for (std::size_t i = 0; i < results.size(); ++i) {
            const PatientRecord* patient = results[i];
            const int row = static_cast<int>(i);
            setCell(searchTable_, row, 0, fromUtf8(patient->id()));
            setCell(searchTable_, row, 1, fromUtf8(patient->name()));
            setCell(searchTable_, row, 2, QString::number(patient->age()));
            setCell(searchTable_, row, 3, fromUtf8(patient->contact()));
            setCell(searchTable_, row, 4, fromUtf8(patient->category()));
            setCell(searchTable_, row, 5, fromUtf8(patient->careGuidance()));
        }
        statusBar()->showMessage(
            QString("Search complete: %1 result(s)").arg(results.size()), 5000);
    } catch (const std::exception& error) {
        showError(error);
    }
}

void MainWindow::sortPatients() {
    const SortAlgorithm algorithm =
        sortAlgorithmInput_->currentIndex() == 0 ? SortAlgorithm::Selection
                                                 : SortAlgorithm::Insertion;
    const SortField field =
        sortFieldInput_->currentIndex() == 0 ? SortField::PatientId
                                             : SortField::Name;
    const SearchEngine::PatientView sorted =
        manager_.sortedPatients(algorithm, field);
    refreshPatients(&sorted);
    searchTable_->setRowCount(static_cast<int>(sorted.size()));
    for (std::size_t i = 0; i < sorted.size(); ++i) {
        const PatientRecord* patient = sorted[i];
        const int row = static_cast<int>(i);
        setCell(searchTable_, row, 0, fromUtf8(patient->id()));
        setCell(searchTable_, row, 1, fromUtf8(patient->name()));
        setCell(searchTable_, row, 2, QString::number(patient->age()));
        setCell(searchTable_, row, 3, fromUtf8(patient->contact()));
        setCell(searchTable_, row, 4, fromUtf8(patient->category()));
        setCell(searchTable_, row, 5, fromUtf8(patient->careGuidance()));
    }
    statusBar()->showMessage("Sorted a patient view; stored records are unchanged.",
                             5000);
}

void MainWindow::saveData() {
    const QString directory = QFileDialog::getExistingDirectory(
        this, "Choose data folder", QDir::homePath(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (directory.isEmpty()) return;
    try {
        manager_.save(toUtf8(directory));
        statusBar()->showMessage("Patient and appointment data saved", 5000);
    } catch (const std::exception& error) {
        showError(error);
    }
}

void MainWindow::loadData() {
    const QString directory = QFileDialog::getExistingDirectory(
        this, "Choose folder containing patients.csv and appointments.csv",
        QDir::homePath(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (directory.isEmpty()) return;
    try {
        loadDataFromDirectory(toUtf8(directory));
        statusBar()->showMessage("Clinic data loaded", 5000);
    } catch (const std::exception& error) {
        showError(error);
    }
}

void MainWindow::loadDataFromDirectory(const std::string& directory) {
    manager_.load(directory);
    refreshAll();
}

void MainWindow::startNewDataset() {
    if (QMessageBox::question(
            this, "New dataset",
            "Clear all patients and appointments? This can be undone once.") !=
        QMessageBox::Yes) {
        return;
    }
    manager_.newDataset();
    refreshAll();
    statusBar()->showMessage("Started a new empty dataset", 5000);
}

void MainWindow::undoLastOperation() {
    if (!manager_.undoLastOperation()) {
        QMessageBox::information(this, "Undo", "There is no operation to undo.");
        return;
    }
    refreshAll();
    statusBar()->showMessage("Last administrative operation undone", 5000);
}

void MainWindow::showError(const std::exception& error) {
    QMessageBox::warning(this, "Could not complete operation",
                         QString::fromUtf8(error.what()));
}

} // namespace scapms
