#include "VectorStreetGraphWorker.h"

#include <atomic>
#include <cstddef>
#include <expected>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>

namespace outshine {
namespace {
constexpr size_t kWorkItemsPerSlice = 1024;
}

VectorStreetGraphWorker::VectorStreetGraphWorker(Ground::VectorStreetGraphBuildJob job)
    : Thread_([this, job = std::move(job)](const std::stop_token &stop) mutable {
        Run(std::move(job), stop);
      }) {}

void VectorStreetGraphWorker::Run(Ground::VectorStreetGraphBuildJob job,
                                  const std::stop_token &stop) {
  while (!stop.stop_requested()) {
    auto advanced = job.Advance(kWorkItemsPerSlice);
    Phase_.store(job.PhaseName(), std::memory_order_relaxed);
    Advances_.fetch_add(1, std::memory_order_relaxed);
    if (!advanced) {
      Result_.emplace(std::unexpected(std::move(advanced.error())));
      break;
    }
    if (!*advanced) { continue; }
    const double longestSliceMs = job.LongestSliceMs();
    auto graph = std::move(job).Take();
    if (!graph) {
      Result_.emplace(std::unexpected(std::string(graph.error())));
    } else {
      Result_.emplace(Completed{.Graph = std::move(*graph), .LongestSliceMs = longestSliceMs});
    }
    break;
  }
  if (!Result_) {
    Result_.emplace(std::unexpected(std::string("street graph candidate canceled")));
  }
  Complete_.store(true, std::memory_order_release);
}

std::optional<std::expected<VectorStreetGraphWorker::Completed, std::string>>
VectorStreetGraphWorker::Collect() {
  if (!Complete()) { return std::nullopt; }
  Thread_.join();
  return std::move(Result_);
}

}
