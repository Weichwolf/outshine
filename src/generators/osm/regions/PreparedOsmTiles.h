#ifndef OUTSHINE_GENERATORS_OSM_REGIONS_PREPAREDOSMTILES_H
#define OUTSHINE_GENERATORS_OSM_REGIONS_PREPAREDOSMTILES_H

#include "OsmField.h"
#include "Tasks.h"
#include "AssetCache.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace outshine::Data {
class SourceSet;
}

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
    // Stops new preparation here; already posted compute workers may each add one package.
    size_t PublicationBytesMost = size_t{128} * 1024u * 1024u;
  };

  [[nodiscard]] static std::expected<std::shared_ptr<PreparedOsmTiles>, std::string>
  Open(const std::string &directory, const Data::SourceSet &sources, Tasks &compute);
  [[nodiscard]] static std::expected<std::shared_ptr<PreparedOsmTiles>, std::string>
  Open(const std::string &directory, const Data::SourceSet &sources, Tasks &compute, Budget budget);
  ~PreparedOsmTiles();
  [[nodiscard]] std::expected<SourceDelivery, std::string_view>
  Acquire(Data::SourceReader &sources, TileAt at, const OsmField &layout);
  [[nodiscard]] Counters Costs() const noexcept;

private:
  using Product = std::shared_ptr<const OsmField>;

  struct Layout {
    int Zoom = 0;
    MvtSchema Schema = MvtSchema::Shortbread;
    std::vector<std::string> Layers;
  };

  struct Work {
    enum class Phase : uint8_t { Lookup, Source, Generate, Publish, Restore, Ready };
    Layout Definition;
    std::vector<std::weak_ptr<const uint8_t>> Owners;
    Phase Stage = Phase::Lookup;
    Tasks::Handle Task = Tasks::kNoTask;
    std::expected<Product, std::string> Result;
    std::optional<PreparedAssetWrite> Publication;
    size_t PublicationBytes = 0;
    bool Generating = false;
    std::atomic_bool Cancelled{false};
    unsigned Retries = 0;
    std::chrono::steady_clock::time_point RetryAt;
  };

  enum class ReadCause : uint8_t { Lookup, Publication };

  PreparedOsmTiles(std::unique_ptr<AssetCache> cache,
                   Tasks &compute,
                   std::string recipe,
                   Budget budget);
  void RetireUnused();
  void Forget(Work &work);
  [[nodiscard]] bool CompleteTask(Work &work);
  [[nodiscard]] std::expected<SourceDelivery, std::string_view>
  Advance(Data::SourceReader &sources,
          TileAt at,
          const std::string &request,
          const std::shared_ptr<Work> &work);
  void Lookup(const std::string &request, Work &work);
  void Publish(const std::string &request, Work &work);
  void Defer(Work &work);
  void SetPublication(Work &work, std::optional<PreparedAssetWrite> publication);
  static void Observe(Work &work, const std::shared_ptr<const uint8_t> &owner);
  [[nodiscard]] std::string RequestKey(TileAt at, const Layout &layout) const;
  [[nodiscard]] std::expected<Product, AssetCacheError>
  Load(const std::string &request, const Layout &layout, ReadCause cause);
  [[nodiscard]] std::expected<Product, std::string>
  Generate(TileAt at, Data::SourceDelivery::Answer landing, const std::string &request, Work &work);

  static constexpr size_t kPackageBytesMost = size_t{64} * 1024 * 1024;
  static constexpr size_t kHeapBytesMost = size_t{128} * 1024 * 1024;
  std::unique_ptr<AssetCache> Cache_;
  Tasks &Compute_;
  std::string Recipe_;
  Budget Budget_;
  size_t Generating_ = 0;
  std::chrono::steady_clock::time_point NextRetire_;
  std::unordered_map<std::string, std::shared_ptr<Work>> Pending_;
  std::atomic_uint64_t Hits_{0}, Misses_{0}, Writes_{0}, ReadBytes_{0};
  std::atomic<double> ReadMs_{0}, GenerationMs_{0};
  std::atomic_uint64_t Generations_{0}, PublicationAttempts_{0}, PublicationDeferred_{0};
  std::atomic_uint64_t PublicationBytes_{0}, PublicationPeakBytes_{0}, PublicationReadBytes_{0};
  std::atomic<double> PublicationMs_{0};
};
}
#endif
