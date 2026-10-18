#ifndef OUTSHINE_GENERATORS_FLORA_PLANTSURFACE_H
#define OUTSHINE_GENERATORS_FLORA_PLANTSURFACE_H

#include <cstdint>
#include <cstddef>
#include <vector>

namespace outshine::Generators {
struct PlantSurfaceSize {
  size_t Vertices = 0, Triangles = 0;
};

struct PlantSurface {
  std::vector<float> Positions, Normals, Texture;
  std::vector<uint32_t> Triangles;
};

[[nodiscard]] bool PlantSurfaceIsOriented(const PlantSurface &mesh) noexcept;
}
#endif
