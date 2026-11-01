#ifndef OUTSHINE_GENERATORS_OSM_REGIONS_PREPAREDOSMTILES_H
#define OUTSHINE_GENERATORS_OSM_REGIONS_PREPAREDOSMTILES_H

#include "OsmField.h"
#include "Tasks.h"
#include "AssetCache.h"
#include "AssetPipeline.h"
#include "SourceSession.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
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
       Budget budget,
       AssetCache::Runtime runtime = {});
  // Cancels demand; the pool releases worker state after its last queued/running use.
  ~PreparedOsmTiles();
  PreparedOsmTiles(const PreparedOsmTiles &) = delete;
  PreparedOsmTiles &operator=(const PreparedOsmTiles &) = delete;
  [[nodiscard]] std::expected<SourceDelivery, std::string_view> Acquire(TileAt at,
                                                                        const OsmField &layout);
  [[nodiscard]] Counters Costs() const noexcept;
  [[nodiscard]] AssetPipeline::Snapshot AdmissionCounts() const noexcept;

private:
  friend class OsmField;
  using Product = std::shared_ptr<const OsmField>;

  struct Layout;
  struct Work;
  struct Production;
  struct Worker;
  struct DecodeJob;

  PreparedOsmTiles(std::unique_ptr<AssetCache> cache,
                   std::shared_ptr<Data::SourceSession> sources,
                   Data::SourceTransport &transport,
                   Tasks &compute,
                   std::string recipe,
                   Budget budget);
  void RetireUnused();
  // A missing window withdraws this field's pending interests without touching other owners.
  void RetainPending(const OsmField &layout, const std::optional<OsmTileWindow> &window);
  [[nodiscard]] static bool DecodeIdle(const Work &work);
  [[nodiscard]] std::expected<SourceDelivery, std::string_view>
  Advance(const std::shared_ptr<Work> &work);
  static constexpr size_t kPackageBytesMost = size_t{64} * 1024 * 1024;
  static constexpr size_t kHeapBytesMost = size_t{128} * 1024 * 1024;
  static constexpr size_t kReceiptBytesMost = 4096;
  static constexpr std::string_view kAbsent = "native OSM source absent";
  static constexpr std::string_view kRefused = "native OSM source refused";
  std::shared_ptr<Worker> Worker_;
  AssetPipeline Pipeline_;
  std::vector<std::shared_ptr<Work>> Pending_;
};
}
#endif
