#ifndef OUTSHINE_HOST_FETCHING_H
#define OUTSHINE_HOST_FETCHING_H

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <thread>

#include "Transport.h"

namespace outshine {

constexpr long kTimeoutUnsaidS = 20;
constexpr size_t kBodyMostMegabytes = 64u;
constexpr unsigned kMegabyteShift = 20u;

class Fetching : public Data::Transport {
public:
  struct Config {
    int ConcurrentTransfers = 8;

    int ConnectionsPerHost = 8;

    size_t MaxRequests = 1024;

    long TimeoutS = kTimeoutUnsaidS;

    std::string UserAgent = "outshine/1 (game engine; non-commercial research)";

    size_t MaxBodyBytes = kBodyMostMegabytes << kMegabyteShift;
  };

  explicit Fetching(Config config);
  ~Fetching() override;
  Fetching(const Fetching &) = delete;
  Fetching &operator=(const Fetching &) = delete;

  [[nodiscard]] Data::Ticket Begin(const std::string &url) override;
  [[nodiscard]] Data::Wire Collect(Data::Ticket ticket) override;
  void Cancel(Data::Ticket ticket) override;
  [[nodiscard]] bool Await(double forMs) override;

  [[nodiscard]] int WorkerCount() const { return Worker_.joinable() ? 1 : 0; }

private:
  struct Transfer {
    uint64_t Ticket = 0;
    std::string Url;
    void *Handle = nullptr;
    std::atomic_bool Cancelled = false;
    size_t MaxBodyBytes = 0;
    bool Done = false;
    bool Unreachable = false;
    int Status = 0;
    double RetryAfterS = 0.0;
    std::vector<uint8_t> Body;
  };

  [[nodiscard]] size_t AddQueuedTransfers(void *multi, size_t active);
  void CollectCompletions(void *multi, size_t &active);
  [[nodiscard]] bool ConfigureTransfer(void *handle, Transfer &transfer) const;
  void FinishTransfers(void *multi, bool failed);
  void Work();
  void WakeWorker() noexcept;

  Config Config_;
  std::mutex Mutex_;
  std::condition_variable Landed_;
  uint64_t Completions_ = 0;
  std::map<uint64_t, Transfer> Transfers_;
  std::deque<uint64_t> Queue_;
  uint64_t NextTicket_ = 1;
  bool Stopping_ = false;
  void *Multi_ = nullptr;
  std::thread Worker_;
};

}
#endif
