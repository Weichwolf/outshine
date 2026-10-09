#ifndef OUTSHINE_GENERATORS_OSM_REGIONS_PREPAREDOSMTILES_H
#define OUTSHINE_GENERATORS_OSM_REGIONS_PREPAREDOSMTILES_H

#include "OsmField.h"
#include "Tasks.h"
#include "content/AssetCache.h"
#include <atomic>
#include <expected>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace outshine::Generators::Osm {
class PreparedOsmTiles {
public:
  enum class State : uint8_t { Pending, Ready, Absent, Refused };

  struct Delivery {
    State Status = State::Pending;
    std::shared_ptr<const OsmField> Product;
  };

  struct Counters {
    uint64_t Hits = 0, Misses = 0, Writes = 0, ReadBytes = 0;
    double ReadMs = 0.0, GenerationMs = 0.0;
  };

  [[nodiscard]] static std::expected<std::shared_ptr<PreparedOsmTiles>, std::string>
  Open(const std::string &directory, const Data::SourceSet &sources, Tasks &compute);
  ~PreparedOsmTiles();
  [[nodiscard]] std::expected<Delivery, std::string_view>
  Acquire(TilePool &tiles, TileAt at, const OsmField &layout);
  [[nodiscard]] Counters Costs() const noexcept;

private:
  using Product = std::shared_ptr<const OsmField>;

  struct Layout {
    int Zoom = 0;
    MvtSchema Schema = MvtSchema::Shortbread;
    std::vector<std::string> Layers;
  };

  struct Work {
    enum class Phase : uint8_t { Lookup, Source, Generate };
    Layout Definition;
    std::vector<std::weak_ptr<const uint8_t>> Owners;
    Phase Stage = Phase::Lookup;
    Tasks::Handle Task = Tasks::kNoTask;
    std::expected<Product, std::string> Result;
  };

  PreparedOsmTiles(std::unique_ptr<AssetCache> cache, Tasks &compute, std::string recipe);
  void RetireUnused();
  static void Observe(Work &work, const std::shared_ptr<const uint8_t> &owner);
  [[nodiscard]] std::string RequestKey(TileAt at, const Layout &layout) const;
  [[nodiscard]] std::expected<Product, std::string> Load(const std::string &request,
                                                         const Layout &layout);
  [[nodiscard]] std::expected<Product, std::string>
  Generate(TileAt at, TilePool::Landing landing, const std::string &request, const Layout &layout);

  static constexpr size_t kPackageBytesMost = size_t{64} * 1024 * 1024;
  static constexpr size_t kHeapBytesMost = size_t{128} * 1024 * 1024;
  static constexpr size_t kPendingMost = 128;
  std::unique_ptr<AssetCache> Cache_;
  Tasks &Compute_;
  std::string Recipe_;
  std::mutex Mutex_;
  std::unordered_map<std::string, std::shared_ptr<Work>> Pending_;
  std::atomic_uint64_t Hits_{0}, Misses_{0}, Writes_{0}, ReadBytes_{0};
  std::atomic<double> ReadMs_{0}, GenerationMs_{0};
};
}
#endif
