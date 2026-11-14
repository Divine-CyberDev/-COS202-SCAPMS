#include "scapms/controller/SearchEngine.h"

#include <cctype>
#include <utility>

namespace scapms {
namespace {

std::string lowercase(std::string value) {
    for (char& character : value) {
        character = static_cast<char>(
            std::tolower(static_cast<unsigned char>(character)));
    }
    return value;
}

bool lessBy(const PatientRecord* left, const PatientRecord* right,
            SortField field) {
    if (field == SortField::PatientId) return left->id() < right->id();
    const std::string leftName = lowercase(left->name());
    const std::string rightName = lowercase(right->name());
    return leftName == rightName ? left->id() < right->id()
                                 : leftName < rightName;
}

} // namespace

const PatientRecord* SearchEngine::linearSearchById(
    const PatientView& patients, const std::string& id) {
    for (const PatientRecord* patient : patients) {
        if (patient->id() == id) return patient;
    }
    return nullptr;
}

const PatientRecord* SearchEngine::binarySearchById(
    const PatientView& patients, const std::string& id) {
    std::size_t first = 0;
    std::size_t last = patients.size();
    while (first < last) {
        const std::size_t middle = first + (last - first) / 2;
        if (patients[middle]->id() == id) return patients[middle];
        if (patients[middle]->id() < id) first = middle + 1;
        else last = middle;
    }
    return nullptr;
}

SearchEngine::PatientView SearchEngine::linearSearchByName(
    const PatientView& patients, const std::string& query) {
    PatientView matches;
    const std::string searchTerm = lowercase(query);
    if (searchTerm.empty()) return matches;
    for (const PatientRecord* patient : patients) {
        if (lowercase(patient->name()).find(searchTerm) != std::string::npos) {
            matches.push_back(patient);
        }
    }
    return matches;
}

void SearchEngine::sort(PatientView& patients, SortAlgorithm algorithm,
                        SortField field) {
    if (algorithm == SortAlgorithm::Selection) {
        for (std::size_t start = 0; start < patients.size(); ++start) {
            std::size_t smallest = start;
            for (std::size_t next = start + 1; next < patients.size(); ++next) {
                if (lessBy(patients[next], patients[smallest], field)) {
                    smallest = next;
                }
            }
            if (smallest != start) std::swap(patients[start], patients[smallest]);
        }
        return;
    }

    for (std::size_t index = 1; index < patients.size(); ++index) {
        const PatientRecord* current = patients[index];
        std::size_t position = index;
        while (position > 0 && lessBy(current, patients[position - 1], field)) {
            patients[position] = patients[position - 1];
            --position;
        }
        patients[position] = current;
    }
}

SearchEngine::PatientView SearchEngine::recursiveSearchByName(
    const PatientView& patients, const std::string& query) {
    PatientView matches;
    if (!query.empty()) {
        recursiveSearch(patients, lowercase(query), 0, matches);
    }
    return matches;
}

void SearchEngine::recursiveSearch(const PatientView& patients,
                                  const std::string& query, std::size_t index,
                                  PatientView& matches) {
    if (index >= patients.size()) return;
    if (lowercase(patients[index]->name()).find(query) != std::string::npos) {
        matches.push_back(patients[index]);
    }
    recursiveSearch(patients, query, index + 1, matches);
}

} // namespace scapms
