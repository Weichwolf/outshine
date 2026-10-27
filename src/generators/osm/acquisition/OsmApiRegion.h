#ifndef OUTSHINE_GENERATORS_OSM_ACQUISITION_OSMAPIREGION_H
#define OUTSHINE_GENERATORS_OSM_ACQUISITION_OSMAPIREGION_H

#include "OsmChunkSetLoader.h"
#include "SourceSet.h"
#include <Provider.h>
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
         Data::RawSourceCache &store,
         Data::SourceTransport &wire,
         const Data::ProviderRegistry *registry,
         std::string_view shippedRoot,
         std::optional<GeoCellId> cell);
  ~ApiRegion();
  [[nodiscard]] SourceChunk Chunk(const Data::SourceProvider &provider,
                                  std::optional<GeoCellId> cell) const;
  [[nodiscard]] bool Begin(const Data::RawSourceCache &store, std::vector<GeoCellId> &refine);
  [[nodiscard]] std::expected<Collected, std::string> Collect(SourceChunk &chunk, double beganMs);

private:
  ApiRegion(Data::RawSourceCache &store, Data::SourceTransport &wire, Data::Address at);
  Data::SourceSet Sources_;
  Data::SourceTransport &Wire_;
  Data::Address Address_;
  std::optional<Data::SourceSet::Query> Query_;
};
}
#endif
