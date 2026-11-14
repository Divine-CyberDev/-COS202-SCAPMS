#pragma once

#include <string>

namespace scapms {

enum class AppointmentStatus { Pending, Completed, Cancelled };

class Appointment {
public:
    Appointment(std::string id, std::string patientId, std::string dateTime,
                std::string reason,
                AppointmentStatus status = AppointmentStatus::Pending);

    const std::string& id() const noexcept;
    const std::string& patientId() const noexcept;
    const std::string& dateTime() const noexcept;
    const std::string& reason() const noexcept;
    AppointmentStatus status() const noexcept;
    void setStatus(AppointmentStatus status) noexcept;
    static std::string statusName(AppointmentStatus status);
    static bool isValidDateTime(const std::string& value);

private:
    std::string id_;
    std::string patientId_;
    std::string dateTime_;
    std::string reason_;
    AppointmentStatus status_;
};

} // namespace scapms
