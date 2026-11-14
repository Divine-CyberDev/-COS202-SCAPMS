#include "scapms/model/EmergencyPatient.h"

namespace scapms {

std::string EmergencyPatient::category() const { return "Emergency"; }
std::string EmergencyPatient::careGuidance() const {
    return "Urgent triage: assess immediately.";
}
std::unique_ptr<PatientRecord> EmergencyPatient::clone() const {
    return std::make_unique<EmergencyPatient>(*this);
}

} // namespace scapms
