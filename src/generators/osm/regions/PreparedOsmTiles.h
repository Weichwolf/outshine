#ifndef OUTSHINE_GENERATORS_OSM_REGIONS_PREPAREDOSMTILES_H
#define OUTSHINE_GENERATORS_OSM_REGIONS_PREPAREDOSMTILES_H

#include "OsmField.h"
#include "Tasks.h"
#include "AssetCache.h"
#include "AssetPipeline.h"
#include "SourceSession.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <stop_token>
#include <vector>

namespace outshine::Generators::Osm {
class PreparedOsmTiles {
public:
  enum class State : uint8_t { Pending, Ready, Absent, Refused };

  struct SourceDelivery {
    State Status = State::Pending;
    std::shared_ptr<const OsmField> Product;
  };

  struct Counters {
    uint64_t Hits = 0, Misses = 0, Writes = 0, ReadBytes = 0;
    double ReadMs = 0.0, GenerationMs = 0.0;
    uint64_t Generations = 0, PublicationAttempts = 0, PublicationDeferred = 0;
    uint64_t PublicationBytes = 0, PublicationPeakBytes = 0, PublicationReadBytes = 0;
    double PublicationMs = 0.0;
  };

  struct Budget {
    size_t PendingMost = 128;
    size_t PublicationBytesMost = size_t{128} * 1024u * 1024u;
    size_t ProductsMost = 4096;
    size_t InputBytesMost = size_t{512} * 1024u * 1024u;
  };

  // The transport and compute pool outlive this service and its unfinished work.
  [[nodiscard]] static std::expected<std::shared_ptr<PreparedOsmTiles>, std::string>
  Open(const std::string &directory,
       std::shared_ptr<Data::SourceSession> sources,
       Data::SourceTransport &transport,
       Tasks &compute);
  [[nodiscard]] static std::expected<std::shared_ptr<PreparedOsmTiles>, std::string>
  Open(const std::string &directory,
       std::shared_ptr<Data::SourceSession> sources,
       Data::SourceTransport &transport,
       Tasks &compute,
       Budget budget);
  ~PreparedOsmTiles();
  [[nodiscard]] std::expected<SourceDelivery, std::string_view> Acquire(TileAt at,
                                                                        const OsmField &layout);
  [[nodiscard]] Counters Costs() const noexcept;
  [[nodiscard]] AssetPipeline::Snapshot AdmissionCounts() const noexcept;

private:
  using Product = std::shared_ptr<const OsmField>;

  struct Layout {
    int Zoom = 0;
    MvtSchema Schema = MvtSchema::Shortbread;
    std::vector<std::string> Layers;
  };

  struct Work {
    Work(std::string request,
         Layout definition,
         const std::shared_ptr<const uint8_t> &owner,
         AssetPipeline::Consumer consumer,
         AssetPipeline::RequestTicket ticket);
    std::string Request;
    Layout Definition;
    std::weak_ptr<const uint8_t> Owner;
    AssetPipeline::Consumer Consumer;
    std::optional<AssetPipeline::RequestTicket> Ticket;
    Tasks::Handle Task = Tasks::kNoTask;
    std::atomic_bool Cancelled{false};
    bool Bound = false;
    bool Retry = false;
    std::string BadDigest;
    bool BadBinding = false;
    std::optional<PreparedAssetWrite> RepairPublication;
    std::optional<AssetProduct> Override;
    std::expected<std::unique_ptr<OsmField>, std::string> Result;
  };

  struct Production {
    enum class Phase : uint8_t {
      Index,
      Hit,
      Source,
      AwaitSource,
      Prepare,
      GenerationAdmission,
      Generate,
      Publish,
      RestorePlan,
      Restore
    };
    PreparedOsmTiles &Owner;
    Layout Definition;
    TileAt At;
    std::string Request;
    Phase Stage = Phase::Index;
    bool SourceAdmitted = false;
    size_t LoadMost = 0;
    size_t SourceMost = 0;
    size_t GenerationMost = 0;
    std::optional<Data::SourceSession::Read> Input;
    std::optional<Data::SourceDelivery::Answer> Landing;
    std::optional<PreparedAssetWrite> Publication;
    std::string ProductKey;
    size_t PublicationBytes = 0;
    ~Production();
  };

  PreparedOsmTiles(std::unique_ptr<AssetCache> cache,
                   std::shared_ptr<Data::SourceSession> sources,
                   Data::SourceTransport &transport,
                   Tasks &compute,
                   std::string recipe,
                   Budget budget);
  void RetireUnused();
  [[nodiscard]] bool CompleteTask(Work &work);
  [[nodiscard]] std::expected<SourceDelivery, std::string_view>
  Advance(const std::shared_ptr<Work> &work);
  [[nodiscard]] std::expected<AssetPipeline::LogicalStep, std::string>
  Resolve(const std::shared_ptr<Production> &work, const std::stop_token &stop);
  [[nodiscard]] std::expected<AssetPipeline::LogicalStep, std::string>
  Produce(Production &work, const std::stop_token &stop);
  [[nodiscard]] std::expected<void, std::string> Generate(Production &work);
  [[nodiscard]] std::expected<AssetPipeline::LogicalStep, std::string>
  PlanGeneration(Production &work);
  [[nodiscard]] std::expected<AssetPipeline::LogicalStep, std::string> Lookup(Production &work);
  [[nodiscard]] std::expected<AssetPipeline::LogicalStep, std::string> LookupHit(Production &work);
  [[nodiscard]] std::expected<AssetPipeline::LogicalStep, std::string>
  OpenSource(const std::shared_ptr<Production> &work);
  [[nodiscard]] std::expected<AssetPipeline::LogicalStep, std::string>
  PollSource(const std::shared_ptr<Production> &work);
  [[nodiscard]] std::expected<AssetPipeline::LogicalStep, std::string>
  PrepareContent(Production &work);
  [[nodiscard]] std::expected<AssetPipeline::LogicalStep, std::string> Publish(Production &work);
  [[nodiscard]] std::expected<AssetPipeline::LogicalStep, std::string>
  PlanRestore(Production &work);
  [[nodiscard]] std::expected<AssetPipeline::LogicalStep, std::string> Restore(Production &work);
  void Decode(Work &work, const AssetPublication &publication);
  [[nodiscard]] bool
  Bind(Work &work, const AssetPublication &publication, AssetPipeline::RequestTicket &ticket);
  [[nodiscard]] bool RestoreBinding(Work &work,
                                    const AssetPublication &publication,
                                    AssetPipeline::RequestTicket &ticket);
  [[nodiscard]] bool
  LoadWinner(Work &work, const AssetRecord &record, AssetPipeline::RequestTicket &ticket);
  [[nodiscard]] static bool
  AdmitOwner(Work &work, AssetPipeline::RequestTicket &ticket, size_t bytes);
  void Repair(Work &work, const CachedAsset &native, AssetPipeline::RequestTicket &ticket);
  [[nodiscard]] bool
  RepairContent(Work &work, const CachedAsset &native, AssetPipeline::RequestTicket &ticket);
  void SetPublication(Production &work, std::optional<PreparedAssetWrite> publication);
  [[nodiscard]] std::string RequestKey(TileAt at, const Layout &layout) const;
  [[nodiscard]] std::string ProductKey(std::string_view request,
                                       std::string_view digest,
                                       std::string_view source,
                                       std::string_view revision) const;
  [[nodiscard]] std::expected<std::optional<CachedAsset>, AssetCacheError>
  Load(const std::string &request, size_t bytesMost);
  [[nodiscard]] bool Matches(const OsmField &field, const Work &work, std::string_view key) const;

  static constexpr size_t kPackageBytesMost = size_t{64} * 1024 * 1024;
  static constexpr size_t kHeapBytesMost = size_t{128} * 1024 * 1024;
  static constexpr size_t kReceiptBytesMost = 4096;
  static constexpr std::string_view kAbsent = "native OSM source absent";
  static constexpr std::string_view kRefused = "native OSM source refused";
  std::unique_ptr<AssetCache> Cache_;
  std::shared_ptr<Data::SourceSession> Sources_;
  Data::SourceTransport &Transport_;
  Tasks &Compute_;
  std::string Recipe_;
  Budget Budget_;
  AssetPipeline Pipeline_;
  std::vector<std::shared_ptr<Work>> Pending_;
  std::atomic_uint64_t Hits_{0}, Misses_{0}, Writes_{0}, ReadBytes_{0};
  std::atomic<double> ReadMs_{0}, GenerationMs_{0};
  std::atomic_uint64_t Generations_{0}, PublicationAttempts_{0}, PublicationDeferred_{0};
  std::atomic_uint64_t PublicationBytes_{0}, PublicationPeakBytes_{0}, PublicationReadBytes_{0};
  std::atomic<double> PublicationMs_{0};
};
}
#endif
