#include "Tasks.h"
#include "Check.h"
#include <chrono>
#include <latch>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Tasks worker(1);
  const auto other = worker.Post([] {});
  CHECK(worker.AwaitCompletion(other, 1), "first task completes without consuming its result");
  std::latch entered(1), release(1);
  const auto asked = worker.Post([&] {
    entered.count_down();
    release.wait();
  });
  entered.wait();
  const auto began = std::chrono::steady_clock::now();
  CHECK(!worker.AwaitCompletion(asked, 0.02),
        "an unrelated retained completion cannot wake a blocked target wait");
  CHECK(std::chrono::steady_clock::now() - began >= std::chrono::milliseconds(10),
        "targeted waiting blocks rather than polling retained unrelated results");
  CHECK(worker.TakeCompletion(other), "waiting leaves the unrelated completion for its owner");
  CHECK(!worker.AwaitCompletion(Tasks::kNoTask, 1) && !worker.AwaitCompletion(asked, 0),
        "invalid handles and empty time budgets do not wait");
  release.count_down();
  CHECK(worker.AwaitCompletion(asked, 1) && worker.TakeCompletion(asked),
        "target completion signals the wait and is consumed only by its owner");
  return Report();
}
