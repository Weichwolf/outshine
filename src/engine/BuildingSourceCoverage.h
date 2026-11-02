#ifndef OUTSHINE_ENGINE_BUILDINGSOURCECOVERAGE_H
#define OUTSHINE_ENGINE_BUILDINGSOURCECOVERAGE_H
#include "TileGeodesy.h"
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace outshine {
namespace Render {
class GeometryPageResidency;
}

namespace Generators::Osm {
class BuildingField;
}

class BuildingSourceCoverage {
public:
  [[nodiscard]] bool Update(const Render::GeometryPageResidency &pieces,
                            const Generators::Osm::BuildingField &footprints);

  [[nodiscard]] std::span<const Ground::GeoBounds> Regions() const noexcept { return Regions_; }

private:
  struct Revision {
    uint64_t Geometry = 0;
    uint64_t Footprints = 0;
    bool operator==(const Revision &) const = default;
  };

  std::optional<Revision> Revision_;
  std::vector<Ground::GeoBounds> Regions_;
};
}
#endif
