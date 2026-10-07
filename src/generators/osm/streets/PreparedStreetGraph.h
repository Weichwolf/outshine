#ifndef OUTSHINE_GENERATORS_OSM_STREETS_PREPAREDSTREETGRAPH_H
#define OUTSHINE_GENERATORS_OSM_STREETS_PREPAREDSTREETGRAPH_H

#include "StreetGraphBuilder.h"
#include "TilePool.h"
#include "content/AssetCache.h"
#include <expected>
#include <functional>
#include <memory>
#include <mutex>
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

  [[nodiscard]] static std::expected<std::shared_ptr<PreparedStreetGraph>, std::string>
  Open(const std::string &directory, const Data::SourceSet &sources);
  [[nodiscard]] std::expected<std::string, std::string>
  Key(const OsmField &vectors,
      const StreetField &ways,
      const ::outshine::Ground::ShapedGround &shape,
      int sourceZoom) const;
  [[nodiscard]] std::expected<Loaded, std::string> Resolve(const std::string &key,
                                                           const Factory &factory);

private:
  PreparedStreetGraph(std::unique_ptr<AssetCache> cache, std::string recipe);
  std::unique_ptr<AssetCache> Cache_;
  std::string Recipe_;
  std::mutex Lock_;
};
}
#endif
