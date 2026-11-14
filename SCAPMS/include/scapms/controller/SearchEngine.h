#pragma once

#include "scapms/model/PatientRecord.h"

#include <string>
#include <vector>

namespace scapms {

enum class SortAlgorithm { Selection, Insertion };
enum class SortField { PatientId, Name };

class SearchEngine {
public:
    using PatientView = std::vector<const PatientRecord*>;

    static const PatientRecord* linearSearchById(const PatientView& patients,
                                                  const std::string& id);
    static const PatientRecord* binarySearchById(const PatientView& patients,
                                                   const std::string& id);
    static PatientView linearSearchByName(const PatientView& patients,
                                          const std::string& query);
    static void sort(PatientView& patients, SortAlgorithm algorithm,
                     SortField field);
    static PatientView recursiveSearchByName(const PatientView& patients,
                                              const std::string& query);

private:
    static void recursiveSearch(const PatientView& patients,
                                const std::string& query, std::size_t index,
                                PatientView& matches);
};

} // namespace scapms
