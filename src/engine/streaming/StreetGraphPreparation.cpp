#include "StreetGraphPreparation.h"

#include <atomic>
#include <cstddef>
#include <chrono>
#include <expected>
#include <memory>
#include <optional>
#include <stop_token>
#include <ratio>
#include <string>
#include <utility>

namespace outshine {
namespace {
constexpr size_t kWorkItemsPerSlice = 1024;
}

struct StreetGraphPreparation::Work {
  explicit Work(Generators::Osm::StreetGraphBuildJob job) : Job(std::move(job)) {}

  Work(JobFactory factory, Resolver resolver)
      : Factory(std::move(factory)), Resolve(std::move(resolver)) {}

  std::optional<Generators::Osm::StreetGraphBuildJob> Job;
  JobFactory Factory;
  Resolver Resolve;
  double LongestSliceMs = 0.0;
  std::optional<std::expected<Completed, std::string>> Result;
  std::atomic<bool> Complete{false};
  std::atomic<const char *> Phase{"begin-weave"};
  std::atomic<size_t> Advances{0};
};

StreetGraphPreparation::StreetGraphPreparation(Tasks &pool,
                                               Generators::Osm::StreetGraphBuildJob job)
    : Work_(std::make_shared<Work>(std::move(job))) {
  Start(pool);
}

StreetGraphPreparation::StreetGraphPreparation(Tasks &pool, JobFactory factory, Resolver resolver)
    : Work_(std::make_shared<Work>(std::move(factory), std::move(resolver))) {
  Start(pool);
}

void StreetGraphPreparation::Start(Tasks &pool) {
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

std::expected<Generators::Osm::StreetGraphBuilder::Built, std::string>
StreetGraphPreparation::Generate(Work &work, const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected("street graph candidate canceled"); }
  if (!work.Job) {
    if (!work.Factory) { return std::unexpected("street graph miss has no job factory"); }
    auto begun = work.Factory();
    if (!begun) { return std::unexpected(std::move(begun.error())); }
    work.Job.emplace(std::move(*begun));
  }
  while (!stop.stop_requested()) {
    auto advanced = work.Job->Advance(kWorkItemsPerSlice);
    work.Phase.store(work.Job->PhaseName(), std::memory_order_relaxed);
    work.Advances.fetch_add(1, std::memory_order_relaxed);
    if (!advanced) { return std::unexpected(std::move(advanced.error())); }
    if (!*advanced) { continue; }
    work.LongestSliceMs = work.Job->LongestSliceMs();
    auto graph = std::move(*work.Job).Take();
    if (!graph) { return std::unexpected(std::string(graph.error())); }
    return std::move(*graph);
  }
  return std::unexpected("street graph candidate canceled");
}

void StreetGraphPreparation::Run(Work &work, const std::stop_token &stop) {
  const auto began = std::chrono::steady_clock::now();
  const Generators::Osm::PreparedStreetGraph::Factory generate = [&] {
    return Generate(work, stop);
  };
  if (stop.stop_requested()) {
    work.Result.emplace(std::unexpected("street graph candidate canceled"));
  } else if (work.Resolve) {
    work.Phase.store("native-lookup", std::memory_order_relaxed);
    auto loaded = work.Resolve(generate);
    if (!loaded) {
      work.Result.emplace(std::unexpected(std::move(loaded.error())));
    } else {
      work.Result.emplace(Completed{.Graph = std::move(loaded->Graph),
                                    .LongestSliceMs = work.LongestSliceMs,
                                    .CacheHit = loaded->Hit,
                                    .ReadBytes = loaded->ReadBytes});
    }
  } else {
    auto built = generate();
    if (!built) {
      work.Result.emplace(std::unexpected(std::move(built.error())));
    } else {
      work.Result.emplace(
          Completed{.Graph = std::move(*built), .LongestSliceMs = work.LongestSliceMs});
    }
  }
  if (stop.stop_requested()) {
    work.Result.emplace(std::unexpected("street graph candidate canceled"));
  }
  if (work.Result && *work.Result) {
    work.Result->value().WorkerMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  }
  work.Complete.store(true, std::memory_order_release);
}

std::optional<std::expected<StreetGraphPreparation::Completed, std::string>>
StreetGraphPreparation::Collect() {
  if (!Complete()) { return std::nullopt; }
  return std::exchange(Work_->Result, std::nullopt);
}

}
