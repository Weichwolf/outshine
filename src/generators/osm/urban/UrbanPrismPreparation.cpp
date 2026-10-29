#include "UrbanPrismPreparation.h"
#include "UrbanPrismPreparationWork.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <ratio>
#include <stop_token>
#include <string>
#include <utility>

namespace outshine::Generators::Osm {
UrbanPrismPreparation::UrbanPrismPreparation(Tasks &pool,
                                             std::string database,
                                             std::string request,
                                             size_t bytesMost)
    : Pool_(pool),
      Database_(std::move(database)),
      Request_(std::move(request)),
      BytesMost_(bytesMost) {
  if (Database_.empty() || Request_.size() != 64 ||
      Request_.find_first_not_of("0123456789abcdef") != std::string::npos || BytesMost_ == 0 ||
      BytesMost_ > kUrbanPrismBytesMost) {
    Result_ = std::unexpected("invalid prism preparation demand or byte budget");
    Phase_ = Phase::Failed;
    return;
  }
  Post(Operation::Read);
}

bool UrbanPrismPreparation::InFlight() const noexcept {
  return Work_ && !Work_->Complete.load(std::memory_order_acquire);
}

bool UrbanPrismPreparation::HoldsAdmission() const noexcept {
  if (InFlight()) { return true; }
  if (Stop_.stop_requested()) { return false; }
  return Phase_ == Phase::Reading || Phase_ == Phase::Building;
}

bool UrbanPrismPreparation::AwaitProgress(double seconds) const {
  if (!Work_) { return true; }
  if (!(seconds > 0.0)) { return Work_->Complete.load(std::memory_order_acquire); }
  std::unique_lock lock(Work_->Mutex);
  return Work_->Landed.wait_for(lock, std::chrono::duration<double>(seconds), [this] {
    return Work_->Complete.load(std::memory_order_acquire);
  });
}

UrbanPrismPreparation::~UrbanPrismPreparation() {
  Cancel();
}

void UrbanPrismPreparation::Cancel() noexcept {
  Stop_.request_stop();
  if (!InFlight()) {
    Work_.reset();
    Pending_.reset();
    Factory_ = {};
    Costs_.PublicationBytes = 0;
  }
}

void UrbanPrismPreparation::Post(Operation operation, AssetFactory factory) {
  Work_ = std::make_shared<Work>();
  Work_->Next = operation;
  Work_->Package = std::exchange(Pending_, std::nullopt);
  Work_->Factory = factory ? std::move(factory) : std::exchange(Factory_, {});
  auto run = [work = Work_,
              stop = Stop_.get_token(),
              database = Database_,
              request = Request_,
              bytesMost = BytesMost_,
              operation] {
    const auto began = std::chrono::steady_clock::now();
    work->Run(database, bytesMost, request, stop);
    const double elapsed =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
    if (operation == Operation::Read) {
      work->Measured.ReadWorkerMs = elapsed;
    } else {
      work->Measured.BuildWorkerMs = elapsed;
    }
    work->Finish();
  };
  if (Pool_.PostDetached(std::move(run))) { return; }
  Work_->Value = std::unexpected("prism preparation compute queue is closed");
  Work_->Package.reset();
  Work_->Factory = {};
  Work_->Finish();
}

void UrbanPrismPreparation::Defer() {
  const auto exponent = std::min(Retries_, 4U);
  Retries_ = std::min(Retries_ + 1U, 4U);
  RetryAt_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(int64_t{1} << exponent);
}

void UrbanPrismPreparation::Collect() {
  Result_ = std::move(Work_->Value);
  Costs_.ReadWorkerMs += Work_->Measured.ReadWorkerMs;
  Costs_.BuildWorkerMs += Work_->Measured.BuildWorkerMs;
  Costs_.ReadBytes += Work_->Measured.ReadBytes;
  Costs_.Generations += Work_->Measured.Generations;
  Costs_.PublicationPreparations += Work_->Measured.PublicationPreparations;
  Costs_.PublicationAttempts += Work_->Measured.PublicationAttempts;
  Pending_ = std::move(Work_->Package);
  Factory_ = std::move(Work_->Factory);
  Costs_.PublicationBytes = Pending_ ? Pending_->CapacityBytes() : 0;
  Next_ = Work_->Next;
  const bool busy = Work_->Busy;
  Work_.reset();
  if (busy) {
    Defer();
  } else if (!Result_) {
    Phase_ = Phase::Failed;
  } else if (*Result_) {
    Phase_ = Phase::Ready;
  } else if (Next_ == Operation::Read) {
    Phase_ = Phase::Inputs;
  } else {
    Phase_ = Phase::Building;
    Retries_ = 0;
    RetryAt_ = std::chrono::steady_clock::time_point{};
  }
}

UrbanPrismPreparation::Result UrbanPrismPreparation::Advance(const Capture &capture,
                                                             bool allowWork) {
  if (Stop_.stop_requested()) {
    Cancel();
    Phase_ = Phase::Failed;
    Result_ = std::unexpected("prism preparation canceled");
  }
  if (Phase_ == Phase::Reading || Phase_ == Phase::Building) {
    if (InFlight()) { return Product{}; }
    if (Work_) { Collect(); }
    if (Phase_ == Phase::Reading || Phase_ == Phase::Building) {
      if (allowWork && std::chrono::steady_clock::now() >= RetryAt_) { Post(Next_); }
      return Product{};
    }
  }
  if (Phase_ == Phase::Ready || Phase_ == Phase::Failed) { return Result_; }
  if (!allowWork || !capture) { return Product{}; }
  const auto began = std::chrono::steady_clock::now();
  auto captured = capture(Stop_.get_token());
  Costs_.CaptureMainMs +=
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  if (!captured) { return Product{}; }
  if (!*captured || !(**captured)) {
    Phase_ = Phase::Failed;
    Result_ = std::unexpected(*captured ? "captured prism factory is empty" : captured->error());
    return Result_;
  }
  Phase_ = Phase::Building;
  Post(Operation::Build, std::move(**captured));
  return Product{};
}
}
