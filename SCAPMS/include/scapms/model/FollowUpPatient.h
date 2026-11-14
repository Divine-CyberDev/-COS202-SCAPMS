#pragma once

#include "scapms/model/PatientRecord.h"

namespace scapms {

class FollowUpPatient final : public PatientRecord {
public:
    using PatientRecord::PatientRecord;
    std::string category() const override;
    std::string careGuidance() const override;
    std::unique_ptr<PatientRecord> clone() const override;
};

} // namespace scapms
