#pragma once

#include "scapms/controller/ClinicDatabase.h"

#include <string>
#include <utility>
#include <vector>

namespace scapms {

class CsvFileHandler {
public:
    using LoadedData =
        std::pair<ClinicDatabase::PatientList, std::vector<Appointment>>;

    static void save(const std::string& directory,
                     const ClinicDatabase::PatientList& patients,
                     const std::vector<Appointment>& appointments);
    static LoadedData load(const std::string& directory);

private:
    static std::string escape(const std::string& field);
    static std::vector<std::string> parseRow(const std::string& row);
};

} // namespace scapms
