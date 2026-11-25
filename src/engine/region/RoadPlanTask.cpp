#include "RoadPlanTask.h"
#include "ProfiledRoadMesher.h"
#include "Tasks.h"
#include "WaterSurfaceBuilder.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <expected>
#include <format>
#include <memory>
#include <optional>
#include <ratio>
#include <string>
#include <utility>

namespace outshine {
namespace {
constexpr size_t kWaysPerStep = 128;
constexpr size_t kNodesPerStep = 64;
}

struct RoadPlanTask::State {
  using Corridors = Generators::Corridors;

  explicit State(Input input) : In(std::move(input)) {}

  Tasks::StepResult Step() {
    if (Stopping.load(std::memory_order_relaxed)) {
      return Finish(std::unexpected("road planning cancelled"));
    }
    const auto began = std::chrono::steady_clock::now();
    if (!Plan && In.Buildings && !In.RoofKeys.empty()) {
      RoofDeck =
          PrepareRoofDeckProfile(*In.Sources, *In.Buildings, In.RoofKeys, In.Frame, Stopping);
    }
    const TriangleBvh surface;
    size_t misses = 0;
    std::optional<EastNorth> firstMiss;
    const Drape drape{.Surface = surface, .Field = [&](EastNorth at) {
                        const auto height = In.Terrain.FieldUpM(In.SheetZoom, at);
                        if (!height) {
                          ++misses;
                          if (!firstMiss) { firstMiss = at; }
                        }
                        return height;
                      }};
    const Corridors::Site site{.Vectors = In.Sources->Vectors.get(),
                               .Ways = In.Sources->Ways,
                               .Materials = In.Materials,
                               .Vegetation = In.SurfaceRecipes,
                               .Network = In.Network.get(),
                               .Standing = In.Frame,
                               .Draped = drape,
                               .Classes = In.Classes,
                               .WaterUpM =
                                   [this](LongitudeLatitude at) {
                                     return Generators::WaterSurfaceUpAt(
                                         In.Sources->WaterBodies, In.Frame, at);
                                   },
                               .CensusAt = In.Began,
                               .EyeLatDeg = In.Eye.LatitudeDeg,
                               .EyeLonDeg = In.Eye.LongitudeDeg,
                               .Projection = In.Projection,
                               .RoofDeck = RoofDeck};
    if (!Plan) { Plan = Corridors::Begin(site); }
    const Generators::ProfiledRoadMesher mesher;
    const auto ready = Corridors(mesher).AdvancePlan(*Plan, site, kWaysPerStep, kNodesPerStep);
    const double elapsed =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
    WorkerMs += elapsed;
    LongestStepMs = std::max(LongestStepMs, elapsed);
    if (!ready) { return Finish(std::unexpected(std::string(ready.error()))); }
    if (firstMiss) {
      const auto *shapes = site.Vectors;
      const int vectorZoom =
          shapes != nullptr && !shapes->Tiles().empty() ? shapes->Tiles().front().Z : -1;
      return Finish(std::unexpected(std::format(
          "road plan reached terrain without a DEM field: east {:.3f} m, north {:.3f} m, "
          "{} misses, sheet zoom {}, DEM zoom {}, OSM zoom {}",
          firstMiss->EastM,
          firstMiss->NorthM,
          misses,
          In.SheetZoom,
          In.DemZoom,
          vectorZoom)));
    }
    if (!*ready) { return Tasks::StepResult::Yield; }
    return Finish(
        Product{.Plan = std::move(Plan), .WorkerMs = WorkerMs, .LongestStepMs = LongestStepMs});
  }

  Tasks::StepResult Finish(Result result) {
    Output.emplace(std::move(result));
    Finished.store(true, std::memory_order_release);
    return Tasks::StepResult::Complete;
  }

  Input In;
  RoadDeckProfile RoofDeck;
  std::unique_ptr<Corridors::Job> Plan;
  std::optional<Result> Output;
  std::atomic_bool Stopping{false};
  std::atomic_bool Finished{false};
  double WorkerMs = 0;
  double LongestStepMs = 0;
};

RoadPlanTask::RoadPlanTask(Tasks &pool, Input input)
    : Pool_(pool), State_(std::make_shared<State>(std::move(input))) {
  if (!Pool_.PostDetachedSteps([state = State_] { return state->Step(); })) {
    (void)State_->Finish(std::unexpected("compute worker rejected road planning"));
  }
}

RoadPlanTask::~RoadPlanTask() {
  State_->Stopping.store(true, std::memory_order_relaxed);
  (void)Pool_.PostDetached([state = std::move(State_)] { (void)state; });
}

std::optional<RoadPlanTask::Result> RoadPlanTask::Take() {
  if (!State_->Finished.load(std::memory_order_acquire)) { return std::nullopt; }
  return std::exchange(State_->Output, std::nullopt);
}

bool RoadPlanTask::AwaitSlice(double seconds) const {
  const auto epoch = Pool_.CompletionEpoch();
  return State_->Finished.load(std::memory_order_acquire) || Pool_.AwaitProgress(epoch, seconds);
}
}
