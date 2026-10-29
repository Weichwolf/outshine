#include "StreetGraphPreparation.h"

#include <atomic>
#include <algorithm>
#include <cstdint>
#include <variant>
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

std::string Describe(const Generators::Osm::PreparedStreetGraph::Failure &failure) {
  if (const auto *message = std::get_if<std::string>(&failure)) { return *message; }
  switch (std::get<AssetCacheError>(failure)) {
    case AssetCacheError::InvalidInput: return "invalid street graph cache input";
    case AssetCacheError::Storage: return "street graph cache storage failed";
    case AssetCacheError::Busy: return "street graph publication is busy";
    case AssetCacheError::UnsupportedVersion: return "unsupported street graph cache version";
    case AssetCacheError::CapacityExceeded: return "street graph exceeds its byte budget";
  }
  return "unknown street graph cache failure";
}
}

struct StreetGraphPreparation::Work {
  explicit Work(Generators::Osm::StreetGraphBuildJob job) : Job(std::move(job)) {}

  Work(JobFactory factory, Resolver resolver)
      : Factory(std::move(factory)), Resolve(std::move(resolver)) {}

  std::optional<Generators::Osm::StreetGraphBuildJob> Job;
  JobFactory Factory;
  Resolver Resolve;
  double LongestSliceMs = 0.0, WorkerMs = 0.0;
  unsigned Retries = 0;
  std::chrono::steady_clock::time_point RetryAt;
  std::optional<std::expected<Completed, std::string>> Result;
  enum class Status { Running, Deferred, CancelRequested, Complete };
  std::atomic<Status> State{Status::Running};
  std::atomic<const char *> Phase{"begin-weave"};
  std::atomic<size_t> Advances{0};

  void Finish() {
    if (Result) {
      Job.reset();
      Factory = {};
      Resolve = {};
    }
    auto expected = Status::Running;
    const auto next = Result ? Status::Complete : Status::Deferred;
    if (State.compare_exchange_strong(
            expected, next, std::memory_order_acq_rel, std::memory_order_acquire)) {
      return;
    }
    Result.emplace(std::unexpected("street graph candidate canceled"));
    Job.reset();
    Factory = {};
    Resolve = {};
    State.store(Status::Complete, std::memory_order_release);
  }
};

StreetGraphPreparation::StreetGraphPreparation(Tasks &pool,
                                               Generators::Osm::StreetGraphBuildJob job)
    : Pool_(pool), Work_(std::make_shared<Work>(std::move(job))) {
  Start(pool);
}

StreetGraphPreparation::StreetGraphPreparation(Tasks &pool, JobFactory factory, Resolver resolver)
    : Pool_(pool), Work_(std::make_shared<Work>(std::move(factory), std::move(resolver))) {
  Start(pool);
}

StreetGraphPreparation::Resolver
StreetGraphPreparation::CachedResolver(std::shared_ptr<Generators::Osm::PreparedStreetGraph> cache,
                                       KeyFactory key) {
  using Native = Generators::Osm::PreparedStreetGraph;
  return
      [cache = std::move(cache),
       key = std::move(key),
       state = std::make_shared<std::optional<Native::Resolution>>()](
          const Native::Factory &factory, const std::stop_token &stop) mutable -> Native::Result {
        if (!*state) {
          if (!key) {
            return std::unexpected(std::string("street graph lookup has no key factory"));
          }
          auto address = key();
          if (!address) { return std::unexpected(std::move(address.error())); }
          state->emplace(std::move(*address));
          key = {};
        }
        return cache->Resolve(**state, factory, stop);
      };
}

void StreetGraphPreparation::Start(Tasks &pool) {
  Work_->State.store(Work::Status::Running, std::memory_order_release);
  if (pool.PostDetached([work = Work_, stop = Stop_.get_token()] { Run(*work, stop); })) { return; }
  Work_->Result.emplace(std::unexpected(std::string("street graph compute queue is closed")));
  Work_->Finish();
}

StreetGraphPreparation::~StreetGraphPreparation() {
  Cancel();
}

void StreetGraphPreparation::Cancel() noexcept {
  Stop_.request_stop();
  auto status = Work_->State.load(std::memory_order_acquire);
  if (status == Work::Status::Running &&
      Work_->State.compare_exchange_strong(status,
                                           Work::Status::CancelRequested,
                                           std::memory_order_acq_rel,
                                           std::memory_order_acquire)) {
    return;
  }
  if (status == Work::Status::Deferred) { Start(Pool_); }
}

bool StreetGraphPreparation::Complete() const noexcept {
  return Work_->State.load(std::memory_order_acquire) == Work::Status::Complete;
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
    work.Job.reset();
    work.Factory = {};
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
    auto loaded = work.Resolve(generate, stop);
    if (!loaded) {
      const auto *kind = std::get_if<AssetCacheError>(&loaded.error());
      if (kind != nullptr && *kind == AssetCacheError::Busy) {
        const auto exponent = std::min(work.Retries, 4U);
        work.Retries = std::min(work.Retries + 1U, 4U);
        work.RetryAt =
            std::chrono::steady_clock::now() + std::chrono::milliseconds(int64_t{1} << exponent);
        work.Phase.store("native-cache-busy", std::memory_order_relaxed);
      } else {
        work.Result.emplace(std::unexpected(Describe(loaded.error())));
      }
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
  work.WorkerMs +=
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  if (work.Result && *work.Result) { work.Result->value().WorkerMs = work.WorkerMs; }
  work.Finish();
}

std::optional<std::expected<StreetGraphPreparation::Completed, std::string>>
StreetGraphPreparation::Collect() {
  const auto status = Work_->State.load(std::memory_order_acquire);
  if (status == Work::Status::Running || status == Work::Status::CancelRequested) {
    return std::nullopt;
  }
  if (status == Work::Status::Complete) { return std::exchange(Work_->Result, std::nullopt); }
  if (std::chrono::steady_clock::now() >= Work_->RetryAt) { Start(Pool_); }
  return std::nullopt;
}

}
