#ifndef OUTSHINE_ENGINE_STREAMING_OSMSOURCELOADERSTATE_H
#define OUTSHINE_ENGINE_STREAMING_OSMSOURCELOADERSTATE_H

#include "OsmSourceLoader.h"
#include "ContentStore.h"
#include "OsmApiReader.h"
#include <condition_variable>
#include <deque>
#include <mutex>
#include <atomic>
#include <math/Units.h>

namespace outshine {
namespace Data {
class OsmCellAcquisition;
}

struct OsmSourceLoader::Access {
  Data::Transport *Wire = nullptr;
  std::string Directory;
  std::unique_ptr<Data::ContentStore> Store;
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

struct OsmSourceLoader::Cells {
  struct Retained {
    std::weak_ptr<const Data::OsmSourceSnapshot> Snapshot;
    size_t ChargedBytes = 0;
  };

  CellLimits Limits;
  std::vector<Data::GeoCellId> Roots;
  std::vector<Data::GeoCellId> PublishedRoots;
  std::vector<Data::GeoCellId> Wanted;
  std::vector<CellSource> Preparing;
  std::vector<CellSource> Published;
  std::vector<Retained> Tracked;
  std::optional<Data::SourceProvider> PublishedProvider;
  std::string PublishedRoot;
  const Data::ProviderRegistry *PublishedRegistry = nullptr;

  [[nodiscard]] size_t ChargedBytes() const noexcept;
  [[nodiscard]] std::vector<Data::GeoCellId>
  NextAcquisitionBatch(std::span<const Data::GeoCellId> assigned = {}) const;
  [[nodiscard]] std::vector<CellSource>
  Reuse(std::span<const Data::GeoCellId> wanted, bool published, bool pending) const;
  [[nodiscard]] std::vector<Data::GeoCellId>
  SelectLeaves(std::span<const Data::GeoCellId> roots, bool published, bool pending) const;
  [[nodiscard]] std::expected<void, std::string> Refine(std::span<const Data::GeoCellId> cells);
  [[nodiscard]] bool Ready() const noexcept;
  [[nodiscard]] std::expected<void, std::string> Stage(std::vector<CellSource> ready);
};

struct OsmSourceLoader::CellPipeline {
  struct Exchange {
    std::mutex Mutex;
    std::condition_variable Changed;
    std::deque<Data::GeoCellId> Requests;
    std::deque<Data::OsmSourceRead> Ready;
    std::string Error;
  };

  std::shared_ptr<Exchange> Shared = std::make_shared<Exchange>();
  std::vector<Data::GeoCellId> Assigned;
  Tasks::Handle Handle = Tasks::kNoTask;
  std::stop_source Stop;
  uint64_t Revision = 0;
  void Acquire(const std::shared_ptr<Access> &access,
               Data::SourceProvider provider,
               const Data::ProviderRegistry *registry,
               std::string root,
               uint64_t revision,
               const std::stop_token &stop) const;
  [[nodiscard]] std::expected<void, std::string>
  StartQueued(Data::OsmCellAcquisition &reader) const;
  [[nodiscard]] bool Deliver(Data::OsmSourceRead ready,
                             Access &access,
                             uint64_t revision,
                             const std::stop_token &stop) const;
  [[nodiscard]] static bool DeadlineExceeded(Access &access, uint64_t revision);
  void Fail(std::string error) const;
};
}

#endif
