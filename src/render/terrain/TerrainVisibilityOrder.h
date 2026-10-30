#ifndef OUTSHINE_RENDER_TERRAIN_TERRAINVISIBILITYORDER_H
#define OUTSHINE_RENDER_TERRAIN_TERRAINVISIBILITYORDER_H

#include <cstddef>
#include <span>

namespace outshine::Render {

struct TerrainVisibleSource {
  size_t Index = 0;
  double Depth = 0;
};

// Residency indices keep instance data and patch bounds in the same permutation.
// Real indices precede virtual indices; uncertain depths fall back to source order.
void OrderTerrainSources(std::span<TerrainVisibleSource> sources, size_t realCount) noexcept;

}
#endif
