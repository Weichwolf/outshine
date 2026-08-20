/* THE WIRE, SUPPLIED BY THE HOST. The library declares `Data::Transport` and calls nothing else, so
 * this is the only file in the tree that knows a transport library exists — `nm -u` over the
 * library's own objects shows no symbol of it.
 *
 * ASYNCHRONOUS BECAUSE THE SEAM IS. Begin enqueues and returns; a small pool of threads performs the
 * transfers; Collect answers Working until one has landed. The pool is what makes several tiles
 * outstanding at once, and it is sized against the tile pool's own width — a transport narrower than
 * its caller turns a set of parallel asks into a queue. */
#ifndef CURLTRANSPORT_H
#define CURLTRANSPORT_H

#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "Transport.h"

namespace outshine::Host {

class CurlTransport : public Data::Transport {
public:
  struct Config {
    /* 0 selects a width derived from this host's hardware concurrency. */
    int Threads = 0;
    /* Seconds. One number, because every upstream in this tree serves a tile of tens of kilobytes. */
    long TimeoutS = 20;
    /* What identifies this application to an upstream. Empty is refused by some of them and is a
     * discourtesy to all of them. */
    std::string UserAgent = "outshine/1 (game engine; non-commercial research)";
    /* A hard ceiling on one body, so a truncated, redirected or hostile response cannot grow a
     * buffer without bound. */
    size_t MaxBodyBytes = 64u << 20;
  };

  explicit CurlTransport(const Config &config);
  ~CurlTransport() override;
  CurlTransport(const CurlTransport &) = delete;
  CurlTransport &operator=(const CurlTransport &) = delete;

  [[nodiscard]] Data::Ticket Begin(const std::string &url) override;
  [[nodiscard]] Data::Wire Collect(Data::Ticket ticket) override;
  void Cancel(Data::Ticket ticket) override;

  [[nodiscard]] int ThreadCount() const { return (int)Threads_.size(); }

private:
  struct Transfer {
    std::string Url;
    bool Done = false;
    bool Cancelled = false;
    bool Unreachable = false;
    int Status = 0;
    std::vector<uint8_t> Body;
  };

  void Work();

  Config Config_;
  std::mutex Mutex_;
  std::condition_variable Wake_;
  std::map<uint64_t, Transfer> Transfers_;
  std::vector<uint64_t> Queue_;
  uint64_t NextTicket_ = 1;
  bool Stopping_ = false;
  std::vector<std::thread> Threads_;
};

} // namespace outshine::Host
#endif
