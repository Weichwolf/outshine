#ifndef OUTSHINE_BASE_IO_TASKS_H
#define OUTSHINE_BASE_IO_TASKS_H

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

namespace outshine {

class Tasks {
public:
  using Job = std::function<void()>;
  using Handle = uint64_t;
  static constexpr Handle kNoTask = 0;

  explicit Tasks(int threads);
  ~Tasks();
  Tasks(const Tasks &) = delete;
  Tasks &operator=(const Tasks &) = delete;

  [[nodiscard]] static int ComputeThreads();

  [[nodiscard]] Handle Post(Job job);
  [[nodiscard]] bool Done(Handle which);
  void Wait(Handle which);

  [[nodiscard]] int Threads() const { return static_cast<int>(Threads_.size()); }

  [[nodiscard]] size_t Pending();
  [[nodiscard]] uint64_t Finished();

private:
  struct Posted {
    Handle Which = kNoTask;
    Job Run;
  };

  void Work();

  std::mutex Mutex_;
  std::condition_variable Wake_;
  std::condition_variable Landed_;
  std::deque<Posted> Queue_;
  std::set<Handle> Done_;
  Handle Next_ = 1;
  uint64_t Finished_ = 0;
  size_t Running_ = 0;
  bool Stopping_ = false;
  std::vector<std::thread> Threads_;
};

} // namespace outshine
#endif
