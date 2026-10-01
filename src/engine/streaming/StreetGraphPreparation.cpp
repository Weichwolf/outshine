#include "StreetGraphPreparation.h"

#include <atomic>
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>

namespace outshine {
namespace {
constexpr size_t kWorkItemsPerSlice = 1024;
}

struct StreetGraphPreparation::Work {
  explicit Work(Ground::StreetGraphBuildJob job) : Job(std::move(job)) {}

  Ground::StreetGraphBuildJob Job;
  std::optional<std::expected<Completed, std::string>> Result;
  std::atomic<bool> Complete{false};
  std::atomic<const char *> Phase{"begin-weave"};
  std::atomic<size_t> Advances{0};
};

StreetGraphPreparation::StreetGraphPreparation(Tasks &pool, Ground::StreetGraphBuildJob job)
    : Work_(std::make_shared<Work>(std::move(job))) {
  if (pool.PostDetached([work = Work_, stop = Stop_.get_token()] { Run(*work, stop); })) { return; }
  Work_->Result.emplace(std::unexpected(std::string("street graph compute queue is closed")));
  Work_->Complete.store(true, std::memory_order_release);
}

StreetGraphPreparation::~StreetGraphPreparation() {
  Cancel();
}

void StreetGraphPreparation::Cancel() noexcept {
  Stop_.request_stop();
}

bool StreetGraphPreparation::Complete() const noexcept {
  return Work_->Complete.load(std::memory_order_acquire);
}

const char *StreetGraphPreparation::PhaseName() const noexcept {
  return Work_->Phase.load(std::memory_order_relaxed);
}

size_t StreetGraphPreparation::Advances() const noexcept {
  return Work_->Advances.load(std::memory_order_relaxed);
}

void StreetGraphPreparation::Run(Work &work, const std::stop_token &stop) {
  while (!stop.stop_requested()) {
    auto advanced = work.Job.Advance(kWorkItemsPerSlice);
    work.Phase.store(work.Job.PhaseName(), std::memory_order_relaxed);
    work.Advances.fetch_add(1, std::memory_order_relaxed);
    if (!advanced) {
      work.Result.emplace(std::unexpected(std::move(advanced.error())));
      break;
    }
    if (!*advanced) { continue; }
    const double longestSliceMs = work.Job.LongestSliceMs();
    auto graph = std::move(work.Job).Take();
    if (!graph) {
      work.Result.emplace(std::unexpected(std::string(graph.error())));
    } else {
      work.Result.emplace(Completed{.Graph = std::move(*graph), .LongestSliceMs = longestSliceMs});
    }
    break;
  }
  if (!work.Result) {
    work.Result.emplace(std::unexpected(std::string("street graph candidate canceled")));
  }
  work.Complete.store(true, std::memory_order_release);
}

std::optional<std::expected<StreetGraphPreparation::Completed, std::string>>
StreetGraphPreparation::Collect() {
  if (!Complete()) { return std::nullopt; }
  return std::exchange(Work_->Result, std::nullopt);
}

}
