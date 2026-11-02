#ifndef OUTSHINE_GENERATORS_OSM_URBAN_PREPAREDURBANAREAS_H
#define OUTSHINE_GENERATORS_OSM_URBAN_PREPAREDURBANAREAS_H

#include "UrbanArea.h"
#include "PreparedOsmTiles.h"
#include "Tasks.h"
#include "AssetCache.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <unordered_map>

namespace outshine::Data {
class SourceSet;
}

namespace outshine::Generators::Osm {
class UrbanPrismPreparation;

class PreparedUrbanAreas {
public:
  class Request {
  public:
    Request(Data::TileId owner, MvtSchema schema, UrbanAreaRecipe recipe = {})
        : Owner_(owner),
          Schema_(schema),
          Recipe_(recipe),
          Demand_(std::make_shared<const uint8_t>(0)) {}

  private:
    friend class PreparedUrbanAreas;
    Data::TileId Owner_;
    MvtSchema Schema_;
    UrbanAreaRecipe Recipe_;
    std::shared_ptr<const uint8_t> Demand_;
  };

  enum class State : uint8_t { Pending, Ready, Absent, Refused };

  struct SourceDelivery {
    State Status = State::Pending;
    std::shared_ptr<const UrbanArea> Product;
    std::string AssetKey;
  };

  struct Budget {
    static constexpr auto DefaultGenerationTime = std::chrono::seconds(30);
    size_t PendingMost = 4;
    size_t SourceBytesMost = size_t{64} * 1024u * 1024u;
    size_t CellsMost = 1048576;
    size_t SourceVerticesMost = 1048576;
    size_t FeaturesMost = 8192;
    std::chrono::milliseconds GenerationTime{DefaultGenerationTime};
  };

  struct Counters {
    uint64_t Hits = 0, Misses = 0, Generations = 0, Writes = 0, ReadBytes = 0;
    uint64_t SourcePolls = 0, GeneratedCells = 0, Deferred = 0;
  };

  [[nodiscard]] static std::expected<std::shared_ptr<PreparedUrbanAreas>, std::string>
  Open(const std::string &directory,
       const Data::SourceSet &sources,
       Tasks &compute,
       std::shared_ptr<PreparedOsmTiles> prepared);
  [[nodiscard]] static std::expected<std::shared_ptr<PreparedUrbanAreas>, std::string>
  Open(const std::string &directory,
       const Data::SourceSet &sources,
       Tasks &compute,
       std::shared_ptr<PreparedOsmTiles> prepared,
       Budget budget,
       AssetCache::Runtime runtime = {});
  ~PreparedUrbanAreas();
  PreparedUrbanAreas(const PreparedUrbanAreas &) = delete;
  PreparedUrbanAreas &operator=(const PreparedUrbanAreas &) = delete;
  void Cancel() noexcept;
  [[nodiscard]] std::unique_ptr<UrbanPrismPreparation> PreparePrism(std::string request,
                                                                    size_t bytesMost);
  [[nodiscard]] std::expected<SourceDelivery, std::string> Acquire(const Request &request);
  [[nodiscard]] Counters Costs() const noexcept;
  [[nodiscard]] bool AwaitProgress(double seconds) const;

private:
  struct Loaded;
  struct Definition;
  struct Work;
  struct Worker;

  PreparedUrbanAreas(std::unique_ptr<AssetCache> cache,
                     std::string recipe,
                     Tasks &compute,
                     std::shared_ptr<PreparedOsmTiles> prepared,
                     Budget budget);
  void RetireUnused();
  static void Observe(Work &work, const std::shared_ptr<const uint8_t> &owner);
  [[nodiscard]] static Data::TileId Parent(Data::TileId owner) noexcept;
  [[nodiscard]] bool Start(const std::string &key, const std::shared_ptr<Work> &work);
  [[nodiscard]] std::expected<SourceDelivery, std::string> GatherSources(Work &work);
  [[nodiscard]] std::expected<SourceDelivery, std::string>
  Advance(const std::string &key, const std::shared_ptr<Work> &work);
  Tasks &Compute_;
  std::shared_ptr<PreparedOsmTiles> Prepared_;
  std::shared_ptr<Worker> Worker_;
  std::unordered_map<std::string, std::shared_ptr<Work>> Pending_;
};
}
#endif
