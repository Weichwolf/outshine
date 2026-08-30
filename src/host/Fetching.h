#ifndef OUTSHINE_HOST_FETCHING_H
#define OUTSHINE_HOST_FETCHING_H

#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "Transport.h"

namespace outshine {

class Fetching : public Data::Transport {
public:
  struct Config {
    int Threads = 0;

    long TimeoutS = 20;

    std::string UserAgent = "outshine/1 (game engine; non-commercial research)";

    size_t MaxBodyBytes = 64u << 20;
  };

  explicit Fetching(const Config &config);
  ~Fetching() override;
  Fetching(const Fetching &) = delete;
  Fetching &operator=(const Fetching &) = delete;

  [[nodiscard]] Data::Ticket Begin(const std::string &url) override;
  [[nodiscard]] Data::Wire Collect(Data::Ticket ticket) override;
  void Cancel(Data::Ticket ticket) override;
  [[nodiscard]] bool Await(double forMs) override;

  [[nodiscard]] int ThreadCount() const { return (int)Threads_.size(); }

private:
  struct Transfer {
    std::string Url;
    bool Done = false;
    bool Cancelled = false;
    bool Unreachable = false;
    int Status = 0;
    double RetryAfterS = 0.0;
    std::vector<uint8_t> Body;
  };

  void Work();

  Config Config_;
  std::mutex Mutex_;
  std::condition_variable Wake_;
  std::condition_variable Landed_;
  uint64_t Completions_ = 0;
  std::map<uint64_t, Transfer> Transfers_;
  std::vector<uint64_t> Queue_;
  uint64_t NextTicket_ = 1;
  bool Stopping_ = false;
  std::vector<std::thread> Threads_;
};

} // namespace outshine
#endif
