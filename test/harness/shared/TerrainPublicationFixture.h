#ifndef OUTSHINE_TEST_TERRAINPUBLICATIONFIXTURE_H
#define OUTSHINE_TEST_TERRAINPUBLICATIONFIXTURE_H

#include "GroundMesher.h"
#include "HeightPageBatch.h"
#include "TangentFrame.h"
#include "TerrainResidency.h"
#include "TerrainSurfacePublication.h"
#include <cstddef>
#include <string>

namespace outshine::Test {

template <typename Advance>
[[nodiscard]] bool FinishTerrainPublication(size_t pages, std::string &error, Advance advance) {
  for (size_t step = 0; step <= pages; ++step) {
    const auto ready = advance();
    if (!ready) {
      error = ready.error();
      return false;
    }
    if (*ready) { return true; }
  }
  error = "terrain publication exceeded its fixture step budget";
  return false;
}

[[nodiscard]] inline bool PublishTerrain(TerrainResidency &terrain,
                                         const Patchwork &patchwork,
                                         const TangentFrame &frame,
                                         std::string &error) {
  return terrain.BeginPublish(patchwork, error) &&
         FinishTerrainPublication(patchwork.Sheets.size(), error, [&] {
           return terrain.AdvancePublish(patchwork, frame, Render::kHeightPageBatchMost);
         });
}

[[nodiscard]] inline bool
PublishTerrain(TerrainSurfacePublication &terrain, Patchwork &patchwork, std::string &error) {
  return terrain.StitchSeams(patchwork, error) && terrain.BeginResidency(patchwork, error) &&
         FinishTerrainPublication(patchwork.Sheets.size(), error, [&] {
           return terrain.AdvanceResidency(patchwork, Render::kHeightPageBatchMost);
         });
}

}
#endif
