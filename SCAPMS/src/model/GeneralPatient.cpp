#include "scapms/model/GeneralPatient.h"

namespace scapms {

std::string GeneralPatient::category() const { return "General"; }
std::string GeneralPatient::careGuidance() const {
    return "Standard consultation queue.";
}
std::unique_ptr<PatientRecord> GeneralPatient::clone() const {
    return std::make_unique<GeneralPatient>(*this);
}

} // namespace scapms
