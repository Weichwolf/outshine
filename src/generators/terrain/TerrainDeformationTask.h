#ifndef OUTSHINE_GENERATORS_TERRAIN_TERRAINDEFORMATIONTASK_H
#define OUTSHINE_GENERATORS_TERRAIN_TERRAINDEFORMATIONTASK_H

#include "Tasks.h"
#include "PreparedTerrainDeformation.h"
#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace outshine::Generators {
class PreparedTerrainAssets;

class TerrainDeformationTask {
public:
  using StampPreparation = std::function<void(std::vector<EarthworkStamp> &)>;

  // The pool outlives this owner and unfinished callbacks; Advance posts due cache work.
  TerrainDeformationTask(Tasks &pool,
                         std::shared_ptr<PreparedTerrainAssets> cache,
                         std::vector<EarthworkStamp> stamps,
                         Patchwork &candidate,
                         TangentFrame frame,
                         TerrainPageLayout layout,
                         double mostEarthworkM,
                         StampPreparation prepare = {});
  ~TerrainDeformationTask();
  TerrainDeformationTask(const TerrainDeformationTask &) = delete;
  TerrainDeformationTask &operator=(const TerrainDeformationTask &) = delete;

  [[nodiscard]] bool Advance() const;
  [[nodiscard]] bool AwaitSlice(double seconds) const;
  [[nodiscard]] std::expected<TerrainDeformationMetrics, std::string> Take(Patchwork &candidate);
  [[nodiscard]] size_t HeapBytes() const noexcept;

private:
  struct State;
  void Post() const;

  Tasks &Pool_;
  std::shared_ptr<State> State_;
};
}

#endif
