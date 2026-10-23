#ifndef OUTSHINE_GENERATORS_OSM_URBAN_PREPAREDURBANAREAS_H
#define OUTSHINE_GENERATORS_OSM_URBAN_PREPAREDURBANAREAS_H

#include "UrbanArea.h"
#include "PreparedOsmTiles.h"
#include "Tasks.h"
#include "content/AssetCache.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace outshine::Data {
class SourceSet;
}

namespace outshine::Generators::Osm {
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

  struct Delivery {
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
       Budget budget);
  ~PreparedUrbanAreas();
  void Cancel() noexcept;
  [[nodiscard]] std::expected<Delivery, std::string> Acquire(TilePool &tiles,
                                                             const Request &request);
  [[nodiscard]] Counters Costs() const noexcept;
  [[nodiscard]] bool AwaitProgress(double seconds) const;

private:
  struct Loaded {
    std::shared_ptr<const UrbanArea> Area;
    std::string Key;
  };

  struct Definition {
    Data::TileId Owner;
    MvtSchema Schema;
    UrbanAreaRecipe Recipe;
  };

  struct Work {
    enum class Phase : uint8_t { Lookup, Source, Generate };
    Definition Input;
    std::vector<std::weak_ptr<const uint8_t>> Owners;
    std::array<std::unique_ptr<OsmField>, 2> Layouts;
    std::array<std::shared_ptr<const OsmField>, 2> Sources;
    size_t SourceCount = 0;
    Phase Stage = Phase::Lookup;
    Tasks::Handle Task = Tasks::kNoTask;
    std::expected<Loaded, std::string> Result;
  };

  PreparedUrbanAreas(std::unique_ptr<AssetCache> cache,
                     std::string recipe,
                     Tasks &compute,
                     std::shared_ptr<PreparedOsmTiles> prepared,
                     Budget budget);
  void RetireUnused();
  static void Observe(Work &work, const std::shared_ptr<const uint8_t> &owner);
  [[nodiscard]] static Data::TileId Parent(Data::TileId owner) noexcept;
  [[nodiscard]] std::string RequestKey(const Definition &input) const;
  [[nodiscard]] std::expected<Loaded, std::string> Load(const std::string &request,
                                                        const Definition &input);
  [[nodiscard]] std::expected<Loaded, std::string> Generate(const std::string &request,
                                                            const Work &work);
  [[nodiscard]] std::expected<Delivery, std::string> GatherSources(TilePool &tiles, Work &work);
  [[nodiscard]] std::expected<Delivery, std::string>
  Advance(TilePool &tiles, const std::string &key, const std::shared_ptr<Work> &work);
  [[nodiscard]] bool Matches(const UrbanArea &area, const Definition &input) const;

  std::unique_ptr<AssetCache> Cache_;
  std::string Recipe_;
  Tasks &Compute_;
  std::shared_ptr<PreparedOsmTiles> Prepared_;
  Budget Budget_;
  std::stop_source Stop_;
  std::mutex CacheMutex_;
  std::unordered_map<std::string, std::shared_ptr<Work>> Pending_;
  std::atomic_uint64_t Hits_{0}, Misses_{0}, Generations_{0}, Writes_{0}, ReadBytes_{0};
  std::atomic_uint64_t SourcePolls_{0}, GeneratedCells_{0}, Deferred_{0};
};
}
#endif
