#ifndef OUTSHINE_GENERATORS_OSM_ACQUISITION_OSMSOURCEACQUISITIONSTATE_H
#define OUTSHINE_GENERATORS_OSM_ACQUISITION_OSMSOURCEACQUISITIONSTATE_H

#include "OsmSourceAcquisition.h"
#include "RawSourceCache.h"
#include "OsmApiReader.h"
#include <condition_variable>
#include <deque>
#include <mutex>
#include <atomic>
#include <variant>
#include <utility>
#include <Units.h>

namespace outshine::Generators::Osm {
class CellAcquisition;

struct AcquisitionProgress {
  std::mutex Mutex;
  std::condition_variable Changed;
};

template <typename State> struct AcquisitionCompletion {
  explicit AcquisitionCompletion(std::shared_ptr<State> state) : Owner(std::move(state)) {}

  ~AcquisitionCompletion() {
    {
      const std::scoped_lock lock(Owner->Progress->Mutex);
      Owner->Finished.store(true, std::memory_order_release);
    }
    Owner->Progress->Changed.notify_all();
  }

  AcquisitionCompletion(const AcquisitionCompletion &) = delete;
  AcquisitionCompletion &operator=(const AcquisitionCompletion &) = delete;
  std::shared_ptr<State> Owner;
};

struct SourceAcquisition::Result {
  std::shared_ptr<AcquisitionProgress> Progress;
  std::atomic_bool Finished{false};
  std::variant<std::monostate, ReadResult, LoadResult, CellLoadResult> Value;
  std::optional<double> ReadMs;
};

struct SourceAcquisition::Access {
  std::shared_ptr<AcquisitionProgress> Progress = std::make_shared<AcquisitionProgress>();
  Data::SourceTransport *Wire = nullptr;
  std::string Directory;
  std::unique_ptr<Data::RawSourceCache> Store;
  const Data::ProviderRegistry *Registry = nullptr;
  uint64_t AcquisitionRevision = 0;
  uint64_t AppliedBudgetRevision = 0;
  double DeadlineMs = 0;
  std::atomic<double> AcquisitionBudgetMs{DefaultAcquisitionBudgetS * kMsPerS};
  std::atomic<uint64_t> BudgetRevision{0};

  [[nodiscard]] double CurrentDeadline(uint64_t revision) {
    const auto budgetRevision = BudgetRevision.load(std::memory_order_acquire);
    if (AcquisitionRevision != revision || AppliedBudgetRevision != budgetRevision) {
      AcquisitionRevision = revision;
      AppliedBudgetRevision = budgetRevision;
      DeadlineMs = Wire->NowMs() + AcquisitionBudgetMs.load(std::memory_order_relaxed);
    }
    return DeadlineMs;
  }
};

struct SourceAcquisition::Cells {
  struct Retained {
    std::weak_ptr<const void> Snapshot;
    size_t ChargedBytes = 0;
  };

  CellLimits Limits;
  std::vector<GeoCellId> Roots;
  std::vector<GeoCellId> PublishedRoots;
  std::vector<GeoCellId> Wanted;
  std::vector<CellSource> Preparing;
  std::vector<CellSource> Published;
  std::vector<Retained> Tracked;
  std::optional<Data::SourceProvider> PublishedProvider;
  std::string PublishedRoot;
  const Data::ProviderRegistry *PublishedRegistry = nullptr;

  [[nodiscard]] size_t ChargedBytes() const noexcept;
  [[nodiscard]] std::vector<GeoCellId>
  NextAcquisitionBatch(std::span<const GeoCellId> assigned = {}) const;
  [[nodiscard]] std::vector<CellSource>
  Reuse(std::span<const GeoCellId> wanted, bool published, bool pending) const;
  [[nodiscard]] std::vector<GeoCellId>
  SelectLeaves(std::span<const GeoCellId> roots, bool published, bool pending) const;
  [[nodiscard]] std::expected<void, std::string> Refine(std::span<const GeoCellId> cells);
  [[nodiscard]] bool Ready() const noexcept;
  [[nodiscard]] std::expected<void, std::string> Stage(std::vector<CellSource> ready);
};

struct SourceAcquisition::CellPipeline {
  struct Exchange {
    std::shared_ptr<AcquisitionProgress> Progress;
    std::atomic_bool Finished{false};
    std::deque<GeoCellId> Requests;
    std::deque<SourceRead> Ready;
    std::string Error;
  };

  std::shared_ptr<Access> Context;
  std::shared_ptr<Exchange> Shared = std::make_shared<Exchange>();
  std::shared_ptr<CellAcquisition> Reader;
  std::vector<GeoCellId> Assigned;
  std::stop_source Stop;
  uint64_t Revision = 0;
  [[nodiscard]] Tasks::StepResult Acquire(const Data::SourceProvider &provider,
                                          const Data::ProviderRegistry *registry,
                                          std::string_view root,
                                          uint64_t revision,
                                          const std::stop_token &stop);
  [[nodiscard]] std::expected<void, std::string> StartQueued(CellAcquisition &reader,
                                                             const std::stop_token &stop) const;
  void Deliver(SourceRead ready) const;
  [[nodiscard]] static bool DeadlineExceeded(Access &access, uint64_t revision);
  [[nodiscard]] Tasks::StepResult Fail(std::string error);
};
}

#endif
