#include "UrbanPrismSlot.h"
#include <cstddef>
#include <expected>
#include <memory>
#include <string>
#include <utility>

namespace outshine {
UrbanPrismSlot::UrbanPrismSlot(Tasks &pool, std::string database, size_t bytesMost)
    : Pool_(pool), Database_(std::move(database)), BytesMost_(bytesMost) {}

bool UrbanPrismSlot::InFlight() const noexcept {
  return (Job_ && Job_->InFlight()) || (Retiring_ && Retiring_->InFlight());
}

bool UrbanPrismSlot::HoldsAdmission() const noexcept {
  return (Job_ && Job_->HoldsAdmission()) || (Retiring_ && Retiring_->HoldsAdmission());
}

bool UrbanPrismSlot::AwaitProgress(double seconds) const {
  if (Retiring_ && Retiring_->InFlight()) { return Retiring_->AwaitProgress(seconds); }
  return !Job_ || Job_->AwaitProgress(seconds);
}

void UrbanPrismSlot::Retire() {
  if (!Job_) { return; }
  Job_->Cancel();
  Retiring_ = std::move(Job_);
  ++Costs_.Canceled;
}

void UrbanPrismSlot::Clear() {
  Retire();
  Wanted_.clear();
  ReadyKey_.clear();
  Ready_.reset();
  if (Retiring_ && !Retiring_->InFlight()) { Retiring_.reset(); }
}

std::expected<bool, std::string>
UrbanPrismSlot::Advance(const std::string &request,
                        const Generators::Osm::UrbanPrismPreparation::Capture &capture,
                        Admission admission) {
  if (request.size() != 64 || request.find_first_not_of("0123456789abcdef") != std::string::npos) {
    return std::unexpected("invalid prism slot request identity");
  }
  if (Wanted_ != request) {
    Retire();
    Wanted_ = request;
  }
  if (Retiring_) {
    if (Retiring_->InFlight()) { return false; }
    Retiring_.reset();
  }
  if (ReadyKey_ == Wanted_) { return false; }
  if (!Job_) {
    if (admission == Admission::CollectOnly) { return false; }
    Job_ = std::make_unique<Generators::Osm::UrbanPrismPreparation>(
        Pool_, Database_, Wanted_, BytesMost_);
    ++Costs_.Started;
  }
  const Generators::Osm::UrbanPrismPreparation::Capture noCapture;
  auto result = Job_->Advance(admission == Admission::AllowWork ? capture : noCapture,
                              admission == Admission::AllowWork);
  if (!result) { return std::unexpected(result.error()); }
  if (!*result) { return false; }
  Ready_ = std::move(*result);
  ReadyKey_ = Wanted_;
  Job_.reset();
  ++Costs_.Replaced;
  return true;
}
}
