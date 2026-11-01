#include "JobAdmission.h"
#include <atomic>
#include <cassert>
#include <memory>
#include <optional>
#include <semaphore>
#include <utility>

namespace outshine {
struct JobAdmission::State {
  explicit State(int maximum) : Available(maximum), Maximum(maximum) {}

  std::counting_semaphore<> Available;
  std::atomic<int> Active{0};
  int Maximum;
};

JobAdmission::JobAdmission(int maximum) {
  assert(maximum > 0 && std::cmp_less_equal(maximum, std::counting_semaphore<>::max()));
  State_ = std::make_shared<State>(maximum);
}

JobAdmission::Lease::Lease(std::shared_ptr<State> state) : State_(std::move(state)) {}

JobAdmission::Lease::Lease(Lease &&other) noexcept = default;

JobAdmission::Lease &JobAdmission::Lease::operator=(Lease &&other) noexcept {
  if (this == &other) { return *this; }
  Release();
  State_ = std::move(other.State_);
  return *this;
}

JobAdmission::Lease::~Lease() {
  Release();
}

void JobAdmission::Lease::Release() noexcept {
  if (!State_) { return; }
  State_->Active.fetch_sub(1, std::memory_order_relaxed);
  State_->Available.release();
  State_.reset();
}

std::optional<JobAdmission::Lease> JobAdmission::TryAcquire() const {
  if (!State_->Available.try_acquire()) { return std::nullopt; }
  State_->Active.fetch_add(1, std::memory_order_relaxed);
  return Lease(State_);
}

int JobAdmission::Active() const noexcept {
  return State_->Active.load(std::memory_order_relaxed);
}

int JobAdmission::Maximum() const noexcept {
  return State_->Maximum;
}
}
