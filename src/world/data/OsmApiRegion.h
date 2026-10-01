#ifndef OUTSHINE_WORLD_DATA_OSMAPIREGION_H
#define OUTSHINE_WORLD_DATA_OSMAPIREGION_H

#include "OsmChunkSetLoader.h"
#include "SourceSet.h"
#include <world/Provider.h>
#include <memory>

namespace outshine::Data {
class OsmApiRegion {
public:
  enum class Collected { Pending, Ready, Refine };
  [[nodiscard]] static std::expected<std::unique_ptr<OsmApiRegion>, std::string>
  Create(const SourceProvider &provider,
         ContentStore &store,
         Transport &wire,
         const ProviderRegistry *registry,
         std::string_view shippedRoot,
         std::optional<GeoCellId> cell);
  ~OsmApiRegion();
  [[nodiscard]] OsmSourceChunk Chunk(const SourceProvider &provider,
                                     std::optional<GeoCellId> cell) const;
  [[nodiscard]] bool Begin(const ContentStore &store, std::vector<GeoCellId> &refine);
  [[nodiscard]] std::expected<Collected, std::string> Collect(OsmSourceChunk &chunk,
                                                              double beganMs);

private:
  OsmApiRegion(ContentStore &store, Transport &wire, Address at);
  SourceSet Sources_;
  Transport &Wire_;
  Address Address_;
  std::optional<SourceSet::Query> Query_;
};
}
#endif
