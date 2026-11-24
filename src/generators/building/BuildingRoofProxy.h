#ifndef OUTSHINE_GENERATORS_BUILDING_BUILDINGROOFPROXY_H
#define OUTSHINE_GENERATORS_BUILDING_BUILDINGROOFPROXY_H

#include "Box.h"
#include <array>
#include <cstdint>
#include <optional>

namespace outshine::Generators {
class BuildingSurface;

struct BuildingRoofProxy {
  using Plane = std::array<float, 4>;

  uint32_t Source = 0;
  Boxf Bounds;
  std::array<Plane, 7> Planes;
  Vec3f WallTint{{1, 1, 1}};

  // Bounds are relative to the tile anchor. Planes are relative to their box centre.
  [[nodiscard]] static std::optional<BuildingRoofProxy> Of(const BuildingSurface &surface,
                                                           uint32_t source);
  [[nodiscard]] Box WiderBounds() const noexcept;
  [[nodiscard]] double DiameterM() const noexcept;
  [[nodiscard]] bool Valid() const noexcept;
};
}
#endif
