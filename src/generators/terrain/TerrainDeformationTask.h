#ifndef OUTSHINE_GENERATORS_TERRAIN_TERRAINDEFORMATIONTASK_H
#define OUTSHINE_GENERATORS_TERRAIN_TERRAINDEFORMATIONTASK_H

#include "Tasks.h"
#include "PreparedTerrainDeformation.h"
#include <expected>
#include <memory>
#include <string>

namespace outshine::Generators {
class PreparedTerrainAssets;

class TerrainDeformationTask {
public:
  TerrainDeformationTask(Tasks &pool,
                         std::shared_ptr<PreparedTerrainAssets> cache,
                         std::vector<EarthworkStamp> stamps,
                         Patchwork &candidate,
                         TangentFrame frame,
                         TerrainPageLayout layout,
                         double mostEarthworkM);
  ~TerrainDeformationTask();
  TerrainDeformationTask(const TerrainDeformationTask &) = delete;
  TerrainDeformationTask &operator=(const TerrainDeformationTask &) = delete;

  [[nodiscard]] bool Advance() const;
  [[nodiscard]] bool AwaitSlice(double seconds) const;
  [[nodiscard]] std::expected<PressedTerrain, std::string> Take(Patchwork &candidate);
  [[nodiscard]] size_t HeapBytes() const noexcept;

private:
  struct State;
  std::shared_ptr<State> State_;
};
}

#endif
