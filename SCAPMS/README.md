# SCAPMS — Smart Clinic Appointment & Patient Management System

A COS202 Programming II student project implemented in C++17 with a Qt 6
Widgets desktop interface. It demonstrates object-oriented design, manually
implemented algorithms, standard data structures, event-driven GUI programming,
validation, exception handling, CSV persistence, and simple clinic reports.

> **Academic/demo use only.** Use fictional records. This teaching application
> has no authentication, encryption, audit controls, or clinical safeguards and
> is not suitable for real patient information or clinical decision-making.

## Build and run

Requirements: CMake 3.21+, a C++17 compiler, and Qt 6 Widgets (6.2+).

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build
ctest --test-dir build --output-on-failure
./build/scapms
```

On systems where Qt 6 is available through the normal package configuration,
the `CMAKE_PREFIX_PATH` option can be omitted. On macOS, install prerequisites
with `brew install cmake qt`.

The GUI starts with an empty clinic. Choose **File → Load data...** and select
`demo_data` to explore fictional sample records. **File → Save data...** writes
`patients.csv` and `appointments.csv` into the selected folder; use **Load data**
to restore them later.

## Features

- **Dashboard:** patient, appointment, and waiting queue totals.
- **Patients:** register, edit, and delete records for General, Emergency, and
  Follow-Up patients. Emergency, Follow-Up, and General rows have distinct
  presentation colors; category-specific note prompts provide dynamic guidance.
- **Appointments:** schedule, cancel, and complete appointments; a timer checks
  for appointments due within 30 minutes.
- **Waiting queue:** FIFO enrollment and calling the next patient.
- **Search & sort:** search IDs linearly or with binary search; search names
  linearly or recursively; filter by category; select Selection Sort or
  Insertion Sort and sort by ID or name.
- **Reports:** completed-visit ranking, pending-appointment patients, patient
  category distribution, and today's appointment totals by status.
- **Persistence:** CSV import/export through folder chooser dialogs. A failed
  load does not replace the current in-memory clinic data.
- **Undo:** the stack restores the most recent patient add/edit/delete,
  appointment schedule/cancel/complete, or new-dataset operation.
- **Shortcuts:** Ctrl+N (new dataset), Ctrl+S (save), Ctrl+O (load), Ctrl+F
  (search), Ctrl+Z (undo), and Ctrl+Q (exit).

## Design and algorithms

The `scapms::model` namespace contains the abstract `PatientRecord` base,
`Appointable` interface, three patient subclasses, and `Appointment`.
`scapms::controller` owns the in-memory database, clinic operations, searching,
sorting, reports, and undo snapshots. `scapms::gui` contains the Qt window, and
`scapms::utils` contains CSV persistence.

`std::vector` owns patient records and appointments; `std::queue` provides FIFO
waiting order; `std::stack` stores complete snapshots for undo; and a fixed
`std::array<std::string, 10>` remembers the most recently accessed patient IDs.
Patient subclasses are owned by `std::unique_ptr<PatientRecord>` and invoked
through base pointers for runtime polymorphism.

- **Linear search:** inspect each patient ID in order; O(n), appropriate for
  small or unsorted clinic lists.
- **Binary search:** manually sort a view by patient ID with Insertion Sort,
  then halve the remaining search interval on each comparison; O(n log n) for
  the preparatory sort plus O(log n) for the search.
- **Recursive name search:** compare one patient's name per call and recurse to
  the next record; O(n) time and O(n) call-stack space.
- **Selection Sort:** repeatedly select the smallest remaining key; O(n²)
  comparisons and simple to demonstrate on small educational datasets.
- **Insertion Sort:** insert each item into the sorted prefix; O(n²) worst-case
  but efficient on short or nearly sorted lists.

Sorting builds a temporary view of pointers and does not reorder the stored
patient records. The GUI uses `QVBoxLayout`, `QHBoxLayout`, `QGridLayout`, and
`QFormLayout` so sections resize with the window. Signal/slot connections handle
button, text, menu, and timer events.

## Project files

- `include/scapms/` — public model, controller, utility, and GUI headers.
- `src/` — implementation and application entry point.
- `tests/ClinicManagerTests.cpp` — focused CTest coverage for core behavior.
- `demo_data/` — fictional sample CSV files for demonstration.

The assessment report, UML diagram, screenshots, and final PDF are submitted
separately; this repository contains the application source and demo data.
