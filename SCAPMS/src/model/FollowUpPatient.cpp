#include "scapms/model/FollowUpPatient.h"

namespace scapms {

std::string FollowUpPatient::category() const { return "Follow-Up"; }
std::string FollowUpPatient::careGuidance() const {
    return "Review prior treatment and progress.";
}
std::unique_ptr<PatientRecord> FollowUpPatient::clone() const {
    return std::make_unique<FollowUpPatient>(*this);
}

} // namespace scapms
