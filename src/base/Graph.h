#ifndef OUTSHINE_BASE_GRAPH_H
#define OUTSHINE_BASE_GRAPH_H

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <string>
#include <vector>

namespace outshine::Work {

inline constexpr size_t kMostSteps = 64;
inline constexpr size_t kMostAfter = 8;

using Does = void (*)(void *);

class Graph {
public:
  explicit Graph(unsigned hands);
  ~Graph();
  Graph(const Graph &) = delete;
  Graph &operator=(const Graph &) = delete;

  void Clears();
  [[nodiscard]] uint32_t Adds(Does does, void *with);
  [[nodiscard]] bool After(uint32_t step, uint32_t earlier);
  [[nodiscard]] bool Runs();

  [[nodiscard]] size_t Steps() const { return Steps_; }

  [[nodiscard]] unsigned Hands() const { return Hands_; }

  [[nodiscard]] const std::string &Error() const { return Error_; }

private:
  struct Step {
    Does Act = nullptr;
    void *With = nullptr;
    uint32_t After[kMostAfter] = {0};
    uint8_t Afters = 0;
    std::atomic<uint32_t> Owed{0};
    uint32_t Feeds[kMostAfter] = {0};
    uint8_t Fed = 0;
  };

  void Serves(bool untilDone);

  Step Held_[kMostSteps];
  size_t Steps_ = 0;
  unsigned Hands_ = 0;
  std::vector<std::thread> Hand_;
  std::vector<uint32_t> Ready_;
  std::mutex Guard_;
  std::condition_variable Woke_;
  std::atomic<size_t> Left_{0};
  bool Closing_ = false;
  std::string Error_;
};

}

#endif
