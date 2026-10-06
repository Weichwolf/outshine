#include "Tasks.h"
#include "Check.h"
#include <atomic>
#include <chrono>
#include <semaphore>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Tasks tasks(1);
  std::atomic<unsigned> state{0}, calls{0}, observed{0};
  std::binary_semaphore finished(0);
  CHECK(tasks.PostDetachedSteps([&] {
    ++calls;
    if (state == 0) { state = 1; }
    if (state == 2) {
      state = 3;
      finished.release();
      return Tasks::StepResult::Complete;
    }
    return Tasks::StepResult::Yield;
  }),
        "detached continuation is admitted by the common worker");
  const auto marker = tasks.Post([&] {
    observed = state.load();
    state = 2;
  });
  tasks.Wait(marker);
  CHECK(finished.try_acquire_for(std::chrono::seconds(2)) && observed == 1 && state == 3 &&
            calls >= 2,
        "detached continuation yields to independent work before its final step");
  CHECK(!tasks.AwaitCompletion(0.01),
        "detached completion leaves no tracked handle or stale wake predicate");
  return Report();
}
