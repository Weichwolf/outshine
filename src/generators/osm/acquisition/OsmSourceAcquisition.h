#ifndef OUTSHINE_GENERATORS_OSM_ACQUISITION_OSMSOURCEACQUISITION_H
#define OUTSHINE_GENERATORS_OSM_ACQUISITION_OSMSOURCEACQUISITION_H

#include "OsmSourceSnapshot.h"
#include "OsmChunkSetLoader.h"
#include <world/data/Transport.h>
#include "Tasks.h"
#include <world/SourceProvider.h>
#include <world/Provider.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>
#include <variant>

namespace outshine::Generators::Osm {

class SourceAcquisition {
public:
  static constexpr double DefaultAcquisitionBudgetS = 10.0;
  [[nodiscard]] std::expected<void, std::string> SetAcquisitionBudget(double seconds);
  enum class Phase : uint8_t { Inactive, Loading, Verifying, Ready, Failed };
  enum class Target : uint8_t { Inputs, Cache };
  enum class CellState : uint8_t { Missing, Validated };

  struct CellLimits {
    size_t CellsMost = 0;
    size_t SnapshotBytesMost = 0;
    int LevelMost = Data::GeoCellId::MaximumLevel;

    [[nodiscard]] bool operator==(const CellLimits &) const = default;
  };

  struct CellSource {
    std::shared_ptr<const Data::OsmSourceSnapshot> Snapshot;
    size_t ChargedBytes = 0;
    Data::GeoCellId Address{};
    CellState State = CellState::Missing;
  };

  struct Workers {
    Tasks &Compute;
    Tasks &Io;
  };

  explicit SourceAcquisition(Workers workers,
                             Data::Transport *wire = nullptr,
                             std::string cacheDirectory = {},
                             Target target = Target::Inputs);

  ~SourceAcquisition();
  SourceAcquisition(const SourceAcquisition &) = delete;
  SourceAcquisition &operator=(const SourceAcquisition &) = delete;

  [[nodiscard]] std::expected<void, std::string>
  Request(std::span<const Data::SourceProvider> providers,
          std::string_view root,
          const Data::ProviderRegistry *registry = nullptr);

  [[nodiscard]] std::expected<void, std::string>
  RequestCells(const Data::SourceProvider &provider,
               std::span<const Data::GeoCellId> cells,
               CellLimits limits,
               std::string_view root,
               const Data::ProviderRegistry *registry = nullptr);
  [[nodiscard]] std::span<const CellSource> CurrentCells() const noexcept;
  [[nodiscard]] size_t CellSnapshotChargeBytes() const noexcept;
  [[nodiscard]] size_t PreparedCellCount() const noexcept;
  [[nodiscard]] size_t RequiredCellCount() const noexcept;
  void Poll();
  [[nodiscard]] bool AwaitSlice(double seconds);

  [[nodiscard]] Phase CurrentPhase() const noexcept { return Phase_; }

  [[nodiscard]] uint64_t PublishedRevision() const noexcept { return PublishedRevision_; }

  [[nodiscard]] std::string_view Error() const noexcept { return Error_; }

  [[nodiscard]] size_t PendingCount() const noexcept {
    return (Pending_ ? 1u : 0u) + (CellPipeline_ ? 1u : 0u);
  }

  [[nodiscard]] const std::shared_ptr<const Data::OsmSourceSnapshot> &Current() const noexcept {
    return Current_;
  }

private:
  static constexpr double MaximumIoAwaitSeconds = 0.005;
  using LoadResult = std::expected<std::shared_ptr<const Data::OsmSourceSnapshot>, std::string>;
  using ReadResult = std::expected<std::vector<Data::OsmSourceChunk>, std::string>;
  using CellLoadResult = std::expected<std::vector<CellSource>, std::string>;
  using CacheProofResult = std::expected<std::monostate, std::string>;

  struct Result {
    std::variant<std::monostate, ReadResult, LoadResult, CellLoadResult, CacheProofResult> Value;
    std::optional<double> ReadMs;
  };

  struct Pending {
    Tasks::Handle Handle = Tasks::kNoTask;
    Tasks *Owner = nullptr;
    uint64_t Revision = 0;
    std::shared_ptr<Result> Output;
    std::stop_source Stop;
  };

  void StartRegionAcquisition();
  void StartCellPipeline();
  void PumpCellPipeline();
  void ConsumeAcquiredCell();
  void QueueMissingCells();
  void ReleaseAssignedCells(std::span<const CellSource> ready);
  void CancelCellPipeline();
  void StartDecode(std::vector<Data::OsmSourceChunk> input,
                   std::stop_source stop,
                   std::optional<double> readMs);
  void CompletePending(Pending finished);
  void CompleteCells(std::vector<CellSource> ready);
  void VerifyCache();
  void PublishCells();
  struct Access;
  struct Cells;
  struct CellPipeline;
  enum class Scope : uint8_t { Region, Cells };
  const Target Target_;
  Tasks *Tasks_;
  Tasks &Io_;
  std::shared_ptr<Access> Access_;
  std::unique_ptr<Cells> Cells_;
  std::vector<Data::SourceProvider> Requested_;
  std::string Root_;
  std::optional<Pending> Pending_;
  std::unique_ptr<CellPipeline> CellPipeline_;
  std::shared_ptr<const Data::OsmSourceSnapshot> Current_;
  std::string Error_;
  uint64_t Revision_ = 0;
  uint64_t PublishedRevision_ = 0;
  Phase Phase_ = Phase::Inactive;
  Scope Scope_ = Scope::Region;
};

}
#endif
