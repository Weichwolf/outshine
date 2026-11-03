#include "MemoryBudget.h"
#include <atomic>
#include <cstddef>
#include <memory>
#include <optional>
#include <utility>

namespace outshine {
struct MemoryBudget::State {
  explicit State(size_t maximum) : Maximum(maximum) {}

  const size_t Maximum;
  std::atomic_size_t Used{0};

  [[nodiscard]] bool Acquire(size_t bytes) noexcept {
    if (bytes == 0) { return true; }
    auto used = Used.load(std::memory_order_acquire);
    constexpr size_t kAttemptsMost = 16;
    for (size_t attempt = 0; attempt < kAttemptsMost; ++attempt) {
      if (bytes > Maximum - used) { return false; }
      if (Used.compare_exchange_strong(
              used, used + bytes, std::memory_order_acq_rel, std::memory_order_acquire)) {
        return true;
      }
    }
    return false;
  }
};

MemoryBudget::MemoryBudget(size_t maximum) : State_(std::make_shared<State>(maximum)) {}

size_t MemoryBudget::Maximum() const noexcept {
  return State_->Maximum;
}

size_t MemoryBudget::Used() const noexcept {
  return State_->Used.load(std::memory_order_acquire);
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
    if (Bytes_ != 0) { State_->Used.fetch_sub(Bytes_, std::memory_order_release); }
    State_ = std::move(other.State_);
    Bytes_ = std::exchange(other.Bytes_, 0);
  }
  return *this;
}

MemoryBudget::Reservation::~Reservation() {
  if (Bytes_ != 0) { State_->Used.fetch_sub(Bytes_, std::memory_order_release); }
}

bool MemoryBudget::Reservation::Resize(size_t bytes) noexcept {
  if (bytes == Bytes_) { return true; }
  if (!State_) { return false; }
  if (bytes > Bytes_) {
    if (!State_->Acquire(bytes - Bytes_)) { return false; }
  } else {
    State_->Used.fetch_sub(Bytes_ - bytes, std::memory_order_release);
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
