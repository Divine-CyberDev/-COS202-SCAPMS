cmake_minimum_required(VERSION 3.21)
project(SCAPMS VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets)

set(SCAPMS_CORE_SOURCES
    src/model/Appointment.cpp
    src/model/PatientRecord.cpp
    src/model/GeneralPatient.cpp
    src/model/EmergencyPatient.cpp
    src/model/FollowUpPatient.cpp
    src/controller/ClinicDatabase.cpp
    src/controller/SearchEngine.cpp
    src/controller/ClinicManager.cpp
    src/utils/CsvFileHandler.cpp
)

add_library(scapms_core ${SCAPMS_CORE_SOURCES})
target_include_directories(scapms_core PUBLIC include)
target_compile_options(scapms_core PRIVATE -Wall -Wextra -Wpedantic)

add_library(scapms_gui
    src/gui/MainWindow.cpp
    include/scapms/gui/MainWindow.h
)
target_include_directories(scapms_gui PUBLIC include)
target_link_libraries(scapms_gui PUBLIC scapms_core Qt6::Widgets)
target_compile_options(scapms_gui PRIVATE -Wall -Wextra -Wpedantic)

add_executable(scapms src/main.cpp)
target_link_libraries(scapms PRIVATE scapms_gui)
target_compile_options(scapms PRIVATE -Wall -Wextra -Wpedantic)

include(CTest)
if(BUILD_TESTING)
    add_executable(scapms_tests tests/ClinicManagerTests.cpp)
    target_link_libraries(scapms_tests PRIVATE scapms_core)
    target_compile_definitions(scapms_tests PRIVATE
        SCAPMS_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/demo_data")
    target_compile_options(scapms_tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME scapms_tests COMMAND scapms_tests)
endif()
