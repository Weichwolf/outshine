#ifndef OUTSHINE_GENERATORS_OSM_ACQUISITION_OSMAPIREGION_H
#define OUTSHINE_GENERATORS_OSM_ACQUISITION_OSMAPIREGION_H

#include "OsmChunkSetLoader.h"
#include "SourceSet.h"
#include <world/Provider.h>
#include <memory>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace outshine::Generators::Osm {
class ApiRegion {
public:
  enum class Collected { Pending, Ready, Refine };
  [[nodiscard]] static std::expected<std::unique_ptr<ApiRegion>, std::string>
  Create(const Data::SourceProvider &provider,
         Data::ContentStore &store,
         Data::Transport &wire,
         const Data::ProviderRegistry *registry,
         std::string_view shippedRoot,
         std::optional<Data::GeoCellId> cell);
  ~ApiRegion();
  [[nodiscard]] Data::OsmSourceChunk Chunk(const Data::SourceProvider &provider,
                                           std::optional<Data::GeoCellId> cell) const;
  [[nodiscard]] bool Begin(const Data::ContentStore &store, std::vector<Data::GeoCellId> &refine);
  [[nodiscard]] std::expected<Collected, std::string> Collect(Data::OsmSourceChunk &chunk,
                                                              double beganMs);

private:
  ApiRegion(Data::ContentStore &store, Data::Transport &wire, Data::Address at);
  Data::SourceSet Sources_;
  Data::Transport &Wire_;
  Data::Address Address_;
  std::optional<Data::SourceSet::Query> Query_;
};
}
#endif
