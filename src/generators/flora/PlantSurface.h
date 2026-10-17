#ifndef OUTSHINE_GENERATORS_FLORA_PLANTSURFACE_H
#define OUTSHINE_GENERATORS_FLORA_PLANTSURFACE_H

#include <cstdint>
#include <vector>

namespace outshine::Generators {
struct PlantSurface {
  std::vector<float> Positions, Normals, Texture;
  std::vector<uint32_t> Triangles;
};

[[nodiscard]] bool PlantSurfaceIsOriented(const PlantSurface &mesh) noexcept;
}
#endif
