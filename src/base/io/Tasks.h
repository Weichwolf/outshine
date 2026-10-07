#ifndef OUTSHINE_BASE_IO_TASKS_H
#define OUTSHINE_BASE_IO_TASKS_H

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <set>
#include <thread>
#include <variant>
#include <vector>

namespace outshine {

class Tasks {
public:
  using Job = std::function<void()>;
  enum class StepResult : uint8_t { Complete, Yield };
  using Step = std::function<StepResult()>;
  using Handle = uint64_t;
  static constexpr Handle kNoTask = 0;

  explicit Tasks(int threads);
  ~Tasks();
  Tasks(const Tasks &) = delete;
  Tasks &operator=(const Tasks &) = delete;

  [[nodiscard]] static int ComputeThreads();

  [[nodiscard]] Handle Post(Job job);
  [[nodiscard]] Handle PostSteps(Step step);
  [[nodiscard]] bool PostDetached(Job job);
  [[nodiscard]] bool PostDetachedSteps(Step step);
  [[nodiscard]] bool TakeCompletion(Handle which);
  [[nodiscard]] bool AwaitCompletion(double seconds);
  [[nodiscard]] bool AwaitCompletion(Handle which, double seconds);
  void Wait(Handle which);

  [[nodiscard]] int Threads() const { return static_cast<int>(Threads_.size()); }

private:
  using WorkItem = std::variant<Job, Step>;

  struct Posted {
    Handle Which = kNoTask;
    WorkItem Run;
    bool Tracked = true;
  };

  void Work();
  [[nodiscard]] Handle Queue(WorkItem work, bool tracked);

  std::mutex Mutex_;
  std::condition_variable Wake_;
  std::condition_variable Landed_;
  std::deque<Posted> Queue_;
  std::set<Handle> Done_;
  Handle Next_ = 1;
  bool Stopping_ = false;
  std::vector<std::thread> Threads_;
};

}
#endif
