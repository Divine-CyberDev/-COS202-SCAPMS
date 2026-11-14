#pragma once

#include <string>
#include <vector>

namespace scapms {

class Appointable {
public:
    virtual ~Appointable() = default;
    virtual void scheduleAppointment(const std::string& appointmentId) = 0;
    virtual bool cancelAppointment(const std::string& appointmentId) = 0;
    virtual const std::vector<std::string>& appointmentIds() const noexcept = 0;
};

} // namespace scapms
