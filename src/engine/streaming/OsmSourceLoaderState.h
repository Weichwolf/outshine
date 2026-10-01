#ifndef OUTSHINE_ENGINE_STREAMING_OSMSOURCELOADERSTATE_H
#define OUTSHINE_ENGINE_STREAMING_OSMSOURCELOADERSTATE_H

#include "OsmSourceLoader.h"
#include "ContentStore.h"
#include <atomic>
#include <math/Units.h>

namespace outshine {

struct OsmSourceLoader::Access {
  Data::Transport *Wire = nullptr;
  std::string Directory;
  std::unique_ptr<Data::ContentStore> Store;
  const Data::ProviderRegistry *Registry = nullptr;
  uint64_t DeadlineRevision = 0;
  double BeganMs = 0;
  std::atomic<double> AcquisitionBudgetMs{DefaultAcquisitionBudgetS * kMsPerS};
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
  [[nodiscard]] std::vector<Data::GeoCellId> NextBatch() const;
  [[nodiscard]] std::vector<CellSource>
  Reuse(std::span<const Data::GeoCellId> wanted, bool published, bool pending) const;
  [[nodiscard]] std::vector<Data::GeoCellId>
  SelectLeaves(std::span<const Data::GeoCellId> roots, bool published, bool pending) const;
  [[nodiscard]] std::expected<void, std::string> Refine(std::span<const Data::GeoCellId> cells);
  [[nodiscard]] bool Ready() const noexcept;
  [[nodiscard]] std::expected<void, std::string> Stage(std::vector<CellSource> ready);
};

}

#endif
