#include "Tasks.h"
#include "Check.h"
#include <atomic>
#include <chrono>
#include <csignal>
#include <memory>
#include <semaphore>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace {
struct ReenterOnRelease {
  outshine::Tasks *Pool;
  std::binary_semaphore *Release;

  ~ReenterOnRelease() {
    Release->release();
    (void)Pool->PostDetached([] {});
  }
};

void RetireQueuedCallbacks() {
  const pid_t child = fork();
  CHECK(child >= 0, "a child process isolates the scheduler shutdown opponent");
  if (child < 0) { return; }
  if (child == 0) {
    std::binary_semaphore started(0);
    std::binary_semaphore release(0);
    auto pool = std::make_unique<outshine::Tasks>(1);
    if (!pool->PostDetached([&] {
          started.release();
          release.acquire();
        }) ||
        !started.try_acquire_for(std::chrono::seconds(3))) {
      _exit(2);
    }
    auto owner = std::make_shared<ReenterOnRelease>(pool.get(), &release);
    if (!pool->PostDetached([owner] {})) { _exit(3); }
    owner.reset();
    pool.reset();
    _exit(0);
  }
  int status = 0;
  pid_t result = 0;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (result == 0 && std::chrono::steady_clock::now() < deadline) {
    result = waitpid(child, &status, WNOHANG);
    if (result == 0) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
  }
  if (result == 0) {
    (void)kill(child, SIGKILL);
    (void)waitpid(child, &status, 0);
  }
  CHECK(result == child && WIFEXITED(status) && WEXITSTATUS(status) == 0,
        "discarded queued callbacks can reenter a stopping pool without a lock deadlock");
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  RetireQueuedCallbacks();
  Tasks tasks(1);
  std::atomic<unsigned> state{0};
  std::atomic<unsigned> calls{0};
  std::atomic<unsigned> observed{0};
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
