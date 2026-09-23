#ifndef OUTSHINE_GENERATORS_TERRAIN_TERRAINPRESS_H
#define OUTSHINE_GENERATORS_TERRAIN_TERRAINPRESS_H

#include <cstddef>
#include <memory>
#include <span>
#include <vector>

#include "GroundMesher.h"
#include "EarthworkPress.h"
#include "TangentFrame.h"
#include "TerrainPage.h"

namespace outshine::Generators {

struct PressedTerrain {
  size_t Nodes = 0;
  size_t Structures = 0;
  size_t Held = 0;
  double DeepestM = 0.0;
  double RaisedM = 0.0;
  EarthworkMetrics Pads;
  EarthworkMetrics Corridors;
  double GatherMs = 0.0;
  double DecideMs = 0.0;
  double BucketMs = 0.0;
  double RejectMs = 0.0;
  double ApplyMs = 0.0;
  double WriteMs = 0.0;
  double FloorsMs = 0.0;
  double LongestGatherMs = 0.0;
  double LongestDecideMs = 0.0;
  double LongestRejectMs = 0.0;
  double LongestInitializeMs = 0.0;
  double LongestApplyMs = 0.0;
  double LongestWriteMs = 0.0;
  double LongestReprojectMs = 0.0;
  double LongestFloorsMs = 0.0;
};

[[nodiscard]] PressedTerrain PressTerrain(std::span<const EarthworkStamp> yields,
                                          Patchwork &candidate,
                                          const TangentFrame &frame,
                                          TerrainPageLayout layout,
                                          double mostEarthworkM);

class TerrainPressJob {
public:
  TerrainPressJob(std::vector<EarthworkStamp> yields,
                  Patchwork &candidate,
                  TangentFrame frame,
                  TerrainPageLayout layout,
                  double mostEarthworkM);
  ~TerrainPressJob();
  TerrainPressJob(const TerrainPressJob &) = delete;
  TerrainPressJob &operator=(const TerrainPressJob &) = delete;
  TerrainPressJob(TerrainPressJob &&) noexcept;
  TerrainPressJob &operator=(TerrainPressJob &&) noexcept;

  [[nodiscard]] bool Advance(size_t sheetsMost, size_t pointsMost);
  [[nodiscard]] PressedTerrain Take() noexcept;
  [[nodiscard]] size_t HeapBytes() const noexcept;

private:
  struct State;
  std::unique_ptr<State> State_;
};

}
#endif
