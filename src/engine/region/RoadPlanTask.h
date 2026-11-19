#ifndef OUTSHINE_ENGINE_REGION_ROADPLANTASK_H
#define OUTSHINE_ENGINE_REGION_ROADPLANTASK_H

#include "Corridors.h"
#include "PublishedRegion.h"
#include "RegionSurfaceRecipes.h"
#include "TerrainInputPreparation.h"
#include <chrono>
#include <expected>
#include <memory>
#include <optional>
#include <string>

namespace outshine {
class Tasks;

class RoadPlanTask {
public:
  struct Input {
    std::shared_ptr<const Generators::Osm::RegionSources> Sources;
    std::shared_ptr<const Path::Network> Network;
    std::shared_ptr<const TerrainClassificationMap> Classes;
    Generators::GroundMaterialCatalog Materials;
    Generators::RegionSurfaceRecipes SurfaceRecipes;
    Generators::TerrainInputPreparation Terrain;
    TangentFrame Frame;
    ProjectedErrorBudget Projection;
    std::chrono::steady_clock::time_point Began;
    LongitudeLatitude Eye;
    int SheetZoom;
    int DemZoom;
  };

  struct Product {
    std::unique_ptr<Generators::Corridors::Job> Plan;
    double WorkerMs;
    double LongestStepMs;
  };

  using Result = std::expected<Product, std::string>;

  RoadPlanTask(Tasks &pool, Input input);
  ~RoadPlanTask();
  RoadPlanTask(const RoadPlanTask &) = delete;
  RoadPlanTask &operator=(const RoadPlanTask &) = delete;

  [[nodiscard]] std::optional<Result> Take();
  [[nodiscard]] bool AwaitSlice(double seconds) const;

private:
  struct State;
  Tasks &Pool_;
  std::shared_ptr<State> State_;
};
}
#endif
