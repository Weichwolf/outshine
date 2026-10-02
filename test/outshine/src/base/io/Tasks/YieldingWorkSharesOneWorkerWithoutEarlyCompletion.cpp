#include "Tasks.h"
#include "Check.h"

#include <atomic>
#include <chrono>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Tasks tasks(1);
  std::atomic<unsigned> state{0}, calls{0}, markerObserved{0};
  std::atomic<bool> expired{false};
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  const auto sliced = tasks.PostSteps([&] {
    ++calls;
    if (state == 0) { state = 1; }
    if (state == 2) {
      state = 3;
      return Tasks::StepResult::Complete;
    }
    if (std::chrono::steady_clock::now() >= deadline) {
      expired = true;
      return Tasks::StepResult::Complete;
    }
    return Tasks::StepResult::Yield;
  });
  const auto marker = tasks.Post([&] {
    markerObserved = state.load();
    state = 2;
  });
  tasks.Wait(marker);
  tasks.Wait(sliced);
  CHECK(!expired && markerObserved == 1 && state == 3 && calls >= 2,
        "one worker executes an independent queued job between continuation steps");
  CHECK(!tasks.TakeCompletion(sliced) && !tasks.TakeCompletion(marker),
        "each stable handle completes exactly once after its final step");
  return Report();
}
