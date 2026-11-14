#include "scapms/model/Appointment.h"

#include <ctime>
#include <iomanip>
#include <sstream>
#include <utility>

namespace scapms {

Appointment::Appointment(std::string id, std::string patientId,
                         std::string dateTime, std::string reason,
                         AppointmentStatus status)
    : id_(std::move(id)), patientId_(std::move(patientId)),
      dateTime_(std::move(dateTime)), reason_(std::move(reason)),
      status_(status) {}

const std::string& Appointment::id() const noexcept { return id_; }
const std::string& Appointment::patientId() const noexcept { return patientId_; }
const std::string& Appointment::dateTime() const noexcept { return dateTime_; }
const std::string& Appointment::reason() const noexcept { return reason_; }
AppointmentStatus Appointment::status() const noexcept { return status_; }
void Appointment::setStatus(AppointmentStatus status) noexcept { status_ = status; }

std::string Appointment::statusName(AppointmentStatus status) {
    switch (status) {
    case AppointmentStatus::Pending: return "Pending";
    case AppointmentStatus::Completed: return "Completed";
    case AppointmentStatus::Cancelled: return "Cancelled";
    }
    return "Unknown";
}

bool Appointment::isValidDateTime(const std::string& value) {
    std::tm parsed{};
    std::istringstream input(value);
    input >> std::get_time(&parsed, "%Y-%m-%d %H:%M");
    if (input.fail() || input.peek() != std::char_traits<char>::eof()) return false;
    parsed.tm_isdst = -1;
    const int year = parsed.tm_year;
    const int month = parsed.tm_mon;
    const int day = parsed.tm_mday;
    const int hour = parsed.tm_hour;
    const int minute = parsed.tm_min;
    if (std::mktime(&parsed) == static_cast<std::time_t>(-1)) return false;
    return parsed.tm_year == year && parsed.tm_mon == month &&
           parsed.tm_mday == day && parsed.tm_hour == hour &&
           parsed.tm_min == minute;
}

} // namespace scapms
