#ifndef OUTSHINE_GENERATORS_TERRAIN_TERRAINREFINEMENT_H
#define OUTSHINE_GENERATORS_TERRAIN_TERRAINREFINEMENT_H

#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <vector>

#include "GroundMesher.h"
#include "TangentFrame.h"
#include "TerrainGrid.h"
#include "TerrainPage.h"
#include "math/Vec3.h"

namespace outshine::Generators {

struct TerrainRefinementDetail {
  Vec3 EyeM;
  double FocalPx = 0.0;
  double OrthographicPxPerM = 0.0;
  double ErrorPx = 1.0;
};

struct TerrainRefinementSource {
  const Sheet *Page = nullptr;
  const outshine::Ground::TerrainField *Heights = nullptr;
};

[[nodiscard]] std::expected<std::vector<Sheet>, std::string>
RefineTerrain(std::span<const TerrainRefinementSource> sources,
              const TangentFrame &frame,
              TerrainPageLayout layout,
              TerrainRefinementDetail detail,
              size_t maximumPatches);

}
#endif
