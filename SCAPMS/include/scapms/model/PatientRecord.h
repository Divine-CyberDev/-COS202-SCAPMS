#pragma once

#include "scapms/model/Appointable.h"

#include <memory>
#include <string>
#include <vector>

namespace scapms {

class PatientRecord : public Appointable {
public:
    PatientRecord(std::string id, std::string name, int age,
                  std::string contact, std::string notes = {});
    ~PatientRecord() override = default;

    const std::string& id() const noexcept;
    const std::string& name() const noexcept;
    int age() const noexcept;
    const std::string& contact() const noexcept;
    const std::string& notes() const noexcept;
    virtual std::string category() const = 0;
    virtual std::string careGuidance() const = 0;
    virtual std::unique_ptr<PatientRecord> clone() const = 0;

    void update(std::string name, int age, std::string contact,
                std::string notes);
    void scheduleAppointment(const std::string& appointmentId) override;
    bool cancelAppointment(const std::string& appointmentId) override;
    const std::vector<std::string>& appointmentIds() const noexcept override;
    void setAppointmentIds(std::vector<std::string> ids);

private:
    std::string id_;
    std::string name_;
    int age_;
    std::string contact_;
    std::string notes_;
    std::vector<std::string> appointmentIds_;
};

} // namespace scapms
