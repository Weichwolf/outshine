#ifndef OUTSHINE_GENERATORS_OSM_REGIONS_PREPAREDOSMTILEWORKER_H
#define OUTSHINE_GENERATORS_OSM_REGIONS_PREPAREDOSMTILEWORKER_H

#include "PreparedOsmTiles.h"
#include <atomic>
#include <stop_token>

namespace outshine::Generators::Osm {
struct PreparedOsmTiles::SourceOutcome {
  Data::SourceDelivery::State Status;
  double RetryAfterMs = 0;
  std::optional<Data::SourceFailure> Failure;
};

struct PreparedOsmTiles::Layout {
  int Zoom = 0;
  MvtSchema Schema = MvtSchema::Shortbread;
  std::vector<std::string> Layers;
};

struct PreparedOsmTiles::Work {
  Work(std::string request,
       Layout definition,
       TileAt at,
       const std::shared_ptr<const uint8_t> &owner,
       AssetPipeline::Consumer consumer,
       AssetPipeline::RequestTicket ticket);
  std::string Request;
  Layout Definition;
  TileAt At;
  std::weak_ptr<const uint8_t> Owner;
  AssetPipeline::Consumer Consumer;
  std::optional<AssetPipeline::RequestTicket> Ticket;
  std::atomic_bool Decoding{false};
  std::atomic_bool Cancelled{false};
  bool Bound = false;
  bool Retry = false;
  std::string BadDigest;
  bool BadBinding = false;
  std::optional<PreparedAssetWrite> RepairPublication;
  std::optional<AssetProduct> Override;
  std::expected<std::unique_ptr<OsmField>, std::string> Result;
};

struct PreparedOsmTiles::Production {
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
    Restore,
    Deliver
  };
  std::shared_ptr<Worker> Owner;
  Layout Definition;
  TileAt At;
  std::string Request;
  Phase Stage = Phase::Index;
  bool SourceAdmitted = false;
  AssetCache::ReadPlan LoadPlan;
  size_t SourceMost = 0;
  size_t GenerationMost = 0;
  std::optional<Data::SourceSession::Read> Input;
  std::optional<Data::SourceDelivery::Answer> Landing;
  std::optional<PreparedAssetWrite> Publication;
  std::string ProductKey;
  size_t PublicationBytes = 0;
  std::optional<AssetProduct> Delivery;
  ~Production();
};

struct PreparedOsmTiles::Worker {
  Worker(std::unique_ptr<AssetCache> cache,
         std::shared_ptr<Data::SourceSession> sources,
         Data::SourceTransport &transport,
         Tasks &compute,
         std::string recipe,
         Budget budget);
  [[nodiscard]] std::string RequestKey(TileAt at, const Layout &layout) const;
  [[nodiscard]] std::expected<AssetPipeline::LogicalStep, AssetPipeline::LogicalFailure>
  Resolve(const std::shared_ptr<Production> &work, const std::stop_token &stop);
  [[nodiscard]] std::expected<AssetPipeline::LogicalStep, std::string>
  Produce(Production &work, const std::stop_token &stop);
  [[nodiscard]] std::expected<void, std::string> Generate(Production &work);
  [[nodiscard]] std::expected<AssetPipeline::LogicalStep, std::string>
  PlanGeneration(Production &work);
  [[nodiscard]] std::expected<AssetPipeline::LogicalStep, std::string>
  PrepareDelivery(Production &work, CachedAsset native);
  [[nodiscard]] static std::expected<AssetPipeline::LogicalStep, std::string>
  Deliver(Production &work);
  [[nodiscard]] std::expected<AssetPipeline::LogicalStep, std::string> Lookup(Production &work);
  [[nodiscard]] std::expected<AssetPipeline::LogicalStep, std::string> LookupHit(Production &work);
  [[nodiscard]] std::expected<AssetPipeline::LogicalStep, AssetPipeline::LogicalFailure>
  OpenSource(const std::shared_ptr<Production> &work);
  [[nodiscard]] std::expected<AssetPipeline::LogicalStep, AssetPipeline::LogicalFailure>
  PollSource(const std::shared_ptr<Production> &work) const;
  [[nodiscard]] std::expected<AssetPipeline::LogicalStep, std::string>
  PrepareContent(Production &work) const;
  [[nodiscard]] std::expected<AssetPipeline::LogicalStep, std::string> Publish(Production &work);
  [[nodiscard]] std::expected<AssetPipeline::LogicalStep, std::string>
  PlanRestore(Production &work) const;
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
  void Repair(Work &work, const CachedAsset &native, AssetPipeline::RequestTicket &ticket) const;
  [[nodiscard]] bool
  RepairContent(Work &work, const CachedAsset &native, AssetPipeline::RequestTicket &ticket) const;
  void SetPublication(Production &work, std::optional<PreparedAssetWrite> publication);
  [[nodiscard]] std::string ProductKey(std::string_view request,
                                       std::string_view digest,
                                       std::string_view source,
                                       std::string_view revision) const;
  [[nodiscard]] std::expected<std::optional<CachedAsset>, AssetCacheError>
  Load(const std::string &request, const AssetCache::ReadPlan &plan) const;
  [[nodiscard]] bool Matches(const OsmField &field, const Work &work, std::string_view key) const;

  std::unique_ptr<AssetCache> Cache_;
  std::shared_ptr<Data::SourceSession> Sources_;
  Data::SourceTransport &Transport_;
  Tasks &Compute_;
  std::string Recipe_;
  Budget Budget_;
  std::atomic_uint64_t Hits_{0}, Misses_{0}, Writes_{0}, ReadBytes_{0};
  std::atomic<double> ReadMs_{0}, GenerationMs_{0};
  std::atomic_uint64_t Generations_{0}, PublicationAttempts_{0}, PublicationDeferred_{0};
  std::atomic_uint64_t PublicationBytes_{0}, PublicationPeakBytes_{0}, PublicationReadBytes_{0};
  std::atomic<double> PublicationMs_{0};
};

struct PreparedOsmTiles::DecodeJob {
  DecodeJob(std::shared_ptr<Worker> context,
            std::shared_ptr<Work> input,
            AssetPublication publication);
  DecodeJob(const DecodeJob &) = delete;
  DecodeJob &operator=(const DecodeJob &) = delete;
  std::shared_ptr<Worker> Context;
  std::shared_ptr<Work> Input;
  AssetPublication Publication;
  ~DecodeJob();
  void Run() const;
};
}
#endif
