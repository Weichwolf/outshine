#include "Task.h"

namespace outshine::Control {

Doing Task::Step(double dtS) {
  if (Left_) { return Doing::Abandoned; }
  if (Under_ != nullptr) {
    const Doing was = Under_->Step(dtS);
    if (was == Doing::Running) { return Doing::Running; }
    Under_.reset();
    if (was == Doing::Abandoned) {
      Left_ = true;
      return Doing::Abandoned;
    }
  }
  return Act(dtS);
}

void Task::Abandon() {
  Under_.reset();
  Left_ = true;
}

size_t Task::Deep() const {
  return Under_ == nullptr ? 1u : 1u + Under_->Deep();
}

} // namespace outshine::Control
