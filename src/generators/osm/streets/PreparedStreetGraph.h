#ifndef OUTSHINE_GENERATORS_OSM_STREETS_PREPAREDSTREETGRAPH_H
#define OUTSHINE_GENERATORS_OSM_STREETS_PREPAREDSTREETGRAPH_H

#include "ShapedGround.h"

#include "StreetGraphBuilder.h"
#include "AssetCache.h"
#include <array>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <utility>
#include <variant>
#include <string>

namespace outshine::Data {
class SourceSet;
}

namespace outshine::Generators::Osm {

class PreparedStreetGraph {
public:
  using Factory = std::function<std::expected<StreetGraphBuilder::Built, std::string>()>;

  struct Loaded {
    StreetGraphBuilder::Built Graph;
    bool Hit = false;
    size_t ReadBytes = 0;
  };

  using Failure = std::variant<AssetCacheError, std::string>;
  using Result = std::expected<Loaded, Failure>;

  class Resolution {
  public:
    explicit Resolution(std::string key) : Key_(std::move(key)) {}

    // Prepared package capacity; generated native network storage is separate.
    [[nodiscard]] size_t PublicationBytes() const noexcept {
      return Package_ ? Package_->CapacityBytes() : 0;
    }

    [[nodiscard]] uint64_t Preparations() const noexcept { return Preparations_; }

    [[nodiscard]] uint64_t PublicationAttempts() const noexcept { return Attempts_; }

  private:
    friend class PreparedStreetGraph;
    enum class Phase { Lookup, Generate, Publish, Restore, Complete };
    std::string Key_;
    Phase Current_ = Phase::Lookup;
    std::optional<StreetGraphBuilder::Built> Generated_;
    std::optional<PreparedAssetWrite> Package_;
    uint64_t Preparations_ = 0, Attempts_ = 0;
  };

  [[nodiscard]] static std::expected<std::shared_ptr<PreparedStreetGraph>, std::string>
  Open(const std::string &directory, const Data::SourceSet &sources);
  [[nodiscard]] std::expected<std::string, std::string>
  Key(const OsmField &vectors,
      const StreetField &ways,
      const ::outshine::Ground::ShapedGround &shape,
      int sourceZoom,
      const std::array<uint8_t, 32> &terrainBinding) const;
  [[nodiscard]] Result
  Resolve(Resolution &state, const Factory &factory, const std::stop_token &stop = {});

private:
  PreparedStreetGraph(std::unique_ptr<AssetCache> cache, std::string recipe);
  std::unique_ptr<AssetCache> Cache_;
  std::string Recipe_;
  [[nodiscard]] Result
  Resume(Resolution &state, const Factory &factory, const std::stop_token &stop);
  [[nodiscard]] std::expected<void, Failure> Prepare(Resolution &state, const Factory &factory);
  [[nodiscard]] Result Restore(Resolution &state);
};
}
#endif
