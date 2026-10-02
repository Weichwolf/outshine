#include "EngineHeld.h"
#include "WorldInstanceSink.h"
#include "InstancePlacementInputs.h"
#include "ClassStructure.h"

#include <cstddef>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine {

namespace Says {
constexpr auto InstanceBudget = "generated instance count exceeds the prepared output budget";
}

namespace {
constexpr size_t kBaseSnapshotRows = 40;

InstancePlacementInputs PlacementInputs(const Surrounds &world,
                                        const Generators::Tile &region,
                                        LevelOfDetail coarseness,
                                        const Generators::Fields &stands,
                                        const std::shared_ptr<const ClassStructure> &classes) {
  return {.Cell = {region.Zoom(), region.X(), region.Y()},
          .Detail = coarseness,
          .Terrain = world.Stack.Pool().TerrainScopeRevision(),
          .Features = stands.Vectors->Generation(),
          .Footprints = stands.Footprints ? stands.Footprints->Revision() : 0,
          .StreetTiles = stands.Ways ? stands.Ways->IngestedTiles() : 0,
          .WaterTiles = stands.WaterBodies ? stands.WaterBodies->IngestedTiles() : 0,
          .Publication = world.GroundPublished.Current(),
          .Classes = {.Owner = classes},
          .Table = {.Owner = world.Table},
          .FeatureOrigin = {.Owner = stands.Vectors->ShareOriginToken()}};
}

[[nodiscard]] Generators::Fields GenerationFields(const Surrounds &world) {
  if (world.GroundPublished.Current()) {
    if (!world.Region) { return {}; }
    return {.Vectors = world.Region->Vectors(),
            .Footprints = &world.Region->Footprints(),
            .WaterBodies = &world.Region->WaterBodies(),
            .Ways = &world.Region->Ways()};
  }
  return {.Vectors = world.Stack.Vectors(),
          .Footprints = &world.Stack.Footprints(),
          .WaterBodies = &world.Stack.WaterBodies(),
          .Ways = &world.Stack.Ways()};
}
}

bool Engine::State::GenerateInitialInstances(double atLat, double atLon) {
  const Ground::OsmField *const vectors = GenerationFields(World).Vectors;
  Published.RecordMetric(
      "generators: bodies already placed", static_cast<double>(World.Placed), "bodies");
  Published.RecordMetric(
      "generators: a shipped catalogue stands", World.Shipping.Ready() ? 1.0 : 0.0, "yes/no");
  Published.RecordMetric("generators: a ground table stands", World.Table ? 1.0 : 0.0, "yes/no");
  Published.RecordMetric(
      "generators: vector data stands", vectors != nullptr ? 1.0 : 0.0, "yes/no");
  if (World.Placed > 0 || !World.Shipping.Ready() || !World.Table || vectors == nullptr) {
    return false;
  }
  return GenerateInstancesForRegion(
      Generators::Tile::Of(vectors->Zoom(), {.LongitudeDeg = atLon, .LatitudeDeg = atLat}),
      LevelOfDetail::Fine);
}

bool Engine::State::GenerateInstancesForRegion(const Generators::Tile &region,
                                               LevelOfDetail coarseness) {
  const Generators::Fields stands = GenerationFields(World);
  const Ground::OsmField *const vectors = stands.Vectors;
  if (vectors == nullptr) { return false; }
  const auto classes = World.Stack.Classes().Read();
  const auto inputs = PlacementInputs(World, region, coarseness, stands, classes);
  if (World.EmptyPlacementInputs == inputs) { return false; }
  Generators::Ground::Snapshot snapshot;
  const Generators::Snapped how = Generators::SnapshotOver(
      region, World.Stack.Ground(), classes, stands, World.Table, &snapshot);
  World.Reached = static_cast<int>(kBaseSnapshotRows) + (snapshot.Patch ? 1 : 0) +
                  (snapshot.Classes ? 2 : 0) + (snapshot.Features ? 4 : 0);
  Published.RecordMetric("generators: the snapshot",
                         static_cast<double>(static_cast<int>(how)),
                         "0=taken 1=waiting 2=no ground");
  Published.RecordMetric("generators: a patch of ground", snapshot.Patch ? 1.0 : 0.0, "yes/no");
  Published.RecordMetric("generators: land classes", snapshot.Classes ? 1.0 : 0.0, "yes/no");
  Published.RecordMetric("generators: OSM features", snapshot.Features ? 1.0 : 0.0, "yes/no");
  Published.RecordMetric(
      "generators: the region it asks about, x", static_cast<double>(region.X()), "tile");
  Published.RecordMetric("generators: and y", static_cast<double>(region.Y()), "tile");
  Published.RecordMetric("generators: at zoom", static_cast<double>(vectors->Zoom()), "z");
  Published.RecordMetric("generators: vector tiles that settled",
                         static_cast<double>(vectors->Tiles().size()),
                         "tiles");
  Published.RecordMetric(
      "generators: vector tiles it refused", static_cast<double>(vectors->RefusedTiles()), "tiles");
  Published.RecordMetric("generators: that region is settled",
                         vectors->Settled(region.X(), region.Y()) ? 1.0 : 0.0,
                         "yes/no");
  World.Grown = how == Generators::Snapped::Taken;
  if (how != Generators::Snapped::Taken) { return false; }
  const std::optional<Generators::Ground> over =
      Generators::Ground::Of(region, snapshot, coarseness);
  Published.RecordMetric("generators: a ground of that snapshot", over ? 1.0 : 0.0, "yes/no");
  if (!over) { return false; }
  const Generators::RegionPool::Shape shape;
  const Generators::RegionPool::Extent extent{.Reached = over->Where(), .Anywhere = over->Where()};
  Generators::RegionPool pool(extent, shape);
  std::optional<Generators::RegionPool::Lease> lease = pool.TryAcquire(*over);
  Published.RecordMetric("generators: a lease on the region", lease ? 1.0 : 0.0, "yes/no");
  if (!lease) { return false; }
  const Generators::GeneratorSet &placing = World.Shipping.Placing();
  std::vector<Generators::Yield> yields;
  std::vector<std::vector<Generators::Yield::Note>> notes(placing.Count());
  yields.reserve(placing.Count());
  for (size_t at = 0; at < placing.Count(); ++at) {
    const Generators::Making &stood = placing.At(at);
    notes[at].assign(stood.NoteNames().size(), Generators::Yield::Note{});
    yields.emplace_back(lease->Sink(),
                        stood.NoteNames(),
                        std::span<Generators::Yield::Note>(notes[at].data(), notes[at].size()));
  }
  placing.Occupy(*over, std::span<Generators::Yield>(yields.data(), yields.size()));
  size_t placed = 0;
  for (size_t at = 0; at < yields.size(); ++at) {
    const Generators::Yield &one = yields[at];
    const std::string_view called = placing.At(at).Called();
    if (one.Placed().Count > kMaxGeneratedInstances - placed) {
      Error = Says::InstanceBudget;
      return false;
    }
    placed += one.Placed().Count;
    Published.RecordMetric(std::format("generators: {} placed", called),
                           static_cast<double>(one.Placed().Count),
                           "bodies");
    Published.RecordMetric(
        std::format("generators: and {} wanted ground another body already held", called),
        static_cast<double>(one.Claims(Generators::Claim::Outcome::Occupied)),
        "claims");
    Published.RecordMetric(std::format("generators: and {} wanted ground off the region", called),
                           static_cast<double>(one.Claims(Generators::Claim::Outcome::Outside)),
                           "claims");
    Published.RecordMetric(std::format("generators: and {} exhausted the region", called),
                           static_cast<double>(one.Claims(Generators::Claim::Outcome::Full)),
                           "claims");
    for (const Generators::Yield::Note &note : one.Notes()) {
      Published.RecordMetric(std::format("generators: {} {} count", called, note.Name),
                             static_cast<double>(note.Times),
                             "events");
      if (note.Raised) {
        Published.RecordMetric(
            std::format("generators: {} {} peak", called, note.Name), note.Peak, "value");
      }
    }
  }
  Published.RecordMetric("generators: bodies they placed", static_cast<double>(placed), "bodies");
  Published.RecordMetric(
      "generators: makers that were asked", static_cast<double>(placing.Count()), "makers");
  if (placed == 0) {
    World.EmptyPlacementInputs = inputs;
    return false;
  }
  World.EmptyPlacementInputs.reset();
  std::vector<WorldInstance> instances(placed);
  WorldInstanceSink sink(instances, region);
  World.Shipping.Drawing().Draw(*over,
                                placing,
                                std::span<const Generators::Yield>(yields.data(), yields.size()),
                                lease->Sink().Placed(),
                                sink);
  if (sink.Error()) {
    Error = Says::InstanceBudget;
    return false;
  }
  instances.resize(sink.Written());
  World.Instances = std::move(instances);
  World.Placed = placed;
  World.Instanced = World.Instances.size();
  return true;
}

}
