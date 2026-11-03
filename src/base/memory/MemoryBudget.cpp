#include "MemoryBudget.h"
#include <algorithm>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

namespace outshine {
struct MemoryBudget::State {
  explicit State(size_t maximum) : Maximum(maximum) {}

  size_t Maximum;
  size_t Used = 0;
  mutable std::mutex Lock;

  [[nodiscard]] bool Acquire(size_t bytes) noexcept {
    if (bytes == 0) { return true; }
    const std::unique_lock lock(Lock, std::try_to_lock);
    if (!lock || Used > Maximum || bytes > Maximum - Used) { return false; }
    Used += bytes;
    return true;
  }

  void Release(size_t bytes) noexcept {
    const std::scoped_lock lock(Lock);
    Used -= bytes;
  }
};

MemoryBudget::MemoryBudget(size_t maximum) : State_(std::make_shared<State>(maximum)) {}

size_t MemoryBudget::Maximum() const noexcept {
  return Read().Maximum;
}

size_t MemoryBudget::Used() const noexcept {
  return Read().Used;
}

size_t MemoryBudget::Snapshot::Available() const noexcept {
  return Maximum - std::min(Maximum, Used);
}

void MemoryBudget::SetMaximum(size_t maximum) noexcept {
  const std::scoped_lock lock(State_->Lock);
  State_->Maximum = maximum;
}

MemoryBudget::Snapshot MemoryBudget::Read() const noexcept {
  const std::scoped_lock lock(State_->Lock);
  return {.Maximum = State_->Maximum, .Used = State_->Used};
}

size_t MemoryBudget::Available() const noexcept {
  return Read().Available();
}

std::optional<MemoryBudget::Reservation> MemoryBudget::TryReserve(size_t bytes) const noexcept {
  if (!State_->Acquire(bytes)) { return std::nullopt; }
  return Reservation(State_, bytes);
}

MemoryBudget::Reservation::Reservation(std::shared_ptr<State> state, size_t bytes) noexcept
    : State_(std::move(state)), Bytes_(bytes) {}

MemoryBudget::Reservation::Reservation(Reservation &&other) noexcept
    : State_(std::move(other.State_)), Bytes_(std::exchange(other.Bytes_, 0)) {}

MemoryBudget::Reservation &MemoryBudget::Reservation::operator=(Reservation &&other) noexcept {
  if (this != &other) {
    if (Bytes_ != 0) { State_->Release(Bytes_); }
    State_ = std::move(other.State_);
    Bytes_ = std::exchange(other.Bytes_, 0);
  }
  return *this;
}

MemoryBudget::Reservation::~Reservation() {
  if (Bytes_ != 0) { State_->Release(Bytes_); }
}

bool MemoryBudget::Reservation::Resize(size_t bytes) noexcept {
  if (bytes == Bytes_) { return true; }
  if (!State_) { return false; }
  if (bytes > Bytes_) {
    if (!State_->Acquire(bytes - Bytes_)) { return false; }
  } else {
    State_->Release(Bytes_ - bytes);
  }
  Bytes_ = bytes;
  return true;
}

bool MemoryBudget::Reservation::TransferTo(Reservation &target, size_t bytes) noexcept {
  if (bytes > Bytes_ || (target.State_ && target.State_ != State_)) { return false; }
  if (this == &target) { return bytes == 0; }
  if (!target.State_) { target.State_ = State_; }
  target.Bytes_ += bytes;
  Bytes_ -= bytes;
  return true;
}
}
