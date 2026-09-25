#ifndef OUTSHINE_ENGINE_STREAMING_VECTORSTREETGRAPHWORKER_H
#define OUTSHINE_ENGINE_STREAMING_VECTORSTREETGRAPHWORKER_H

#include <atomic>
#include <cstddef>
#include <expected>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>

#include "VectorStreetGraph.h"

namespace outshine {

class VectorStreetGraphWorker {
public:
  struct Completed {
    Ground::VectorStreetGraph::Built Graph;
    double LongestSliceMs = 0.0;
  };

  explicit VectorStreetGraphWorker(Ground::VectorStreetGraphBuildJob job);
  ~VectorStreetGraphWorker() = default;
  VectorStreetGraphWorker(const VectorStreetGraphWorker &) = delete;
  VectorStreetGraphWorker &operator=(const VectorStreetGraphWorker &) = delete;

  void Cancel() noexcept { Thread_.request_stop(); }

  [[nodiscard]] bool Complete() const noexcept { return Complete_.load(std::memory_order_acquire); }

  [[nodiscard]] const char *PhaseName() const noexcept {
    return Phase_.load(std::memory_order_relaxed);
  }

  [[nodiscard]] size_t Advances() const noexcept {
    return Advances_.load(std::memory_order_relaxed);
  }

  [[nodiscard]] std::optional<std::expected<Completed, std::string>> Collect();

private:
  void Run(Ground::VectorStreetGraphBuildJob job, const std::stop_token &stop);

  std::optional<std::expected<Completed, std::string>> Result_;
  std::atomic<bool> Complete_{false};
  std::atomic<const char *> Phase_{"begin-weave"};
  std::atomic<size_t> Advances_{0};
  std::jthread Thread_;
};

}

#endif
