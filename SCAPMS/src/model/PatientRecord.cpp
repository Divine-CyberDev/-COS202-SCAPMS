#include "scapms/model/PatientRecord.h"

#include <algorithm>
#include <utility>

namespace scapms {

PatientRecord::PatientRecord(std::string id, std::string name, int age,
                             std::string contact, std::string notes)
    : id_(std::move(id)), name_(std::move(name)), age_(age),
      contact_(std::move(contact)), notes_(std::move(notes)) {}

const std::string& PatientRecord::id() const noexcept { return id_; }
const std::string& PatientRecord::name() const noexcept { return name_; }
int PatientRecord::age() const noexcept { return age_; }
const std::string& PatientRecord::contact() const noexcept { return contact_; }
const std::string& PatientRecord::notes() const noexcept { return notes_; }

void PatientRecord::update(std::string name, int age, std::string contact,
                           std::string notes) {
    name_ = std::move(name);
    age_ = age;
    contact_ = std::move(contact);
    notes_ = std::move(notes);
}

void PatientRecord::scheduleAppointment(const std::string& appointmentId) {
    if (std::find(appointmentIds_.begin(), appointmentIds_.end(),
                  appointmentId) == appointmentIds_.end()) {
        appointmentIds_.push_back(appointmentId);
    }
}

bool PatientRecord::cancelAppointment(const std::string& appointmentId) {
    const auto found = std::find(appointmentIds_.begin(), appointmentIds_.end(),
                                 appointmentId);
    if (found == appointmentIds_.end()) return false;
    appointmentIds_.erase(found);
    return true;
}

const std::vector<std::string>& PatientRecord::appointmentIds() const noexcept {
    return appointmentIds_;
}

void PatientRecord::setAppointmentIds(std::vector<std::string> ids) {
    appointmentIds_ = std::move(ids);
}

} // namespace scapms
