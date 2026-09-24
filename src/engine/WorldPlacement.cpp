#include "EngineHeld.h"
#include "WorldInstanceSink.h"

#include <cstddef>
#include <format>
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
}

bool Engine::State::GenerateInitialInstances(double atLat, double atLon) {
  Published.Places(
      "generators: bodies already placed", static_cast<double>(World.Placed), "bodies");
  Published.Places(
      "generators: a shipped catalogue stands", World.Shipping.Ready() ? 1.0 : 0.0, "yes/no");
  Published.Places("generators: a ground table stands", World.Table ? 1.0 : 0.0, "yes/no");
  Published.Places(
      "generators: vector data stands", World.Stack.Vectors() != nullptr ? 1.0 : 0.0, "yes/no");
  if (World.Placed > 0 || !World.Shipping.Ready() || !World.Table ||
      World.Stack.Vectors() == nullptr) {
    return false;
  }
  return GenerateInstancesForRegion(
      Generators::Tile::Of(World.Stack.Vectors()->Zoom(),
                           {.LongitudeDeg = atLon, .LatitudeDeg = atLat}),
      LevelOfDetail::Fine);
}

bool Engine::State::GenerateInstancesForRegion(const Generators::Tile &region,
                                               LevelOfDetail coarseness) {
  Generators::Fields stands;
  stands.Vectors = World.Stack.Vectors();
  stands.Footprints = &World.Stack.Footprints();
  stands.WaterBodies = &World.Stack.WaterBodies();
  stands.Ways = &World.Stack.Ways();
  Generators::Ground::Snapshot snapshot;
  const Generators::Snapped how = Generators::SnapshotOver(
      region, World.Stack.Ground(), World.Stack.Classes(), stands, World.Table, &snapshot);
  World.Reached = static_cast<int>(kBaseSnapshotRows) + (snapshot.Patch ? 1 : 0) +
                  (snapshot.Classes ? 2 : 0) + (snapshot.Features ? 4 : 0);
  Published.Places("generators: the snapshot",
                   static_cast<double>(static_cast<int>(how)),
                   "0=taken 1=waiting 2=no ground");
  Published.Places("generators: a patch of ground", snapshot.Patch ? 1.0 : 0.0, "yes/no");
  Published.Places("generators: land classes", snapshot.Classes ? 1.0 : 0.0, "yes/no");
  Published.Places("generators: OSM features", snapshot.Features ? 1.0 : 0.0, "yes/no");
  Published.Places(
      "generators: the region it asks about, x", static_cast<double>(region.X()), "tile");
  Published.Places("generators: and y", static_cast<double>(region.Y()), "tile");
  Published.Places("generators: at zoom", static_cast<double>(World.Stack.Vectors()->Zoom()), "z");
  Published.Places("generators: vector tiles that settled",
                   static_cast<double>(World.Stack.Vectors()->Tiles().size()),
                   "tiles");
  Published.Places("generators: vector tiles it refused",
                   static_cast<double>(World.Stack.Vectors()->RefusedTiles()),
                   "tiles");
  Published.Places("generators: that region is settled",
                   World.Stack.Vectors()->Settled(region.X(), region.Y()) ? 1.0 : 0.0,
                   "yes/no");
  World.Grown = how == Generators::Snapped::Taken;
  if (how != Generators::Snapped::Taken) { return false; }
  const std::optional<Generators::Ground> over =
      Generators::Ground::Of(region, snapshot, coarseness);
  Published.Places("generators: a ground of that snapshot", over ? 1.0 : 0.0, "yes/no");
  if (!over) { return false; }
  const Generators::RegionPool::Shape shape;
  const Generators::RegionPool::Extent extent{.Reached = over->Where(), .Anywhere = over->Where()};
  Generators::RegionPool pool(extent, shape);
  std::optional<Generators::RegionPool::Lease> lease = pool.TryAcquire(*over);
  Published.Places("generators: a lease on the region", lease ? 1.0 : 0.0, "yes/no");
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
    Published.Places(std::format("generators: {} placed", called),
                     static_cast<double>(one.Placed().Count),
                     "bodies");
    Published.Places(
        std::format("generators: and {} wanted ground another body already held", called),
        static_cast<double>(one.Claims(Generators::Claim::Outcome::Occupied)),
        "claims");
    Published.Places(std::format("generators: and {} wanted ground off the region", called),
                     static_cast<double>(one.Claims(Generators::Claim::Outcome::Outside)),
                     "claims");
    Published.Places(std::format("generators: and {} exhausted the region", called),
                     static_cast<double>(one.Claims(Generators::Claim::Outcome::Full)),
                     "claims");
    for (const Generators::Yield::Note &note : one.Notes()) {
      Published.Places(std::format("generators: {} {} count", called, note.Name),
                       static_cast<double>(note.Times),
                       "events");
      if (note.Raised) {
        Published.Places(
            std::format("generators: {} {} peak", called, note.Name), note.Peak, "value");
      }
    }
  }
  Published.Places("generators: bodies they placed", static_cast<double>(placed), "bodies");
  Published.Places(
      "generators: makers that were asked", static_cast<double>(placing.Count()), "makers");
  if (placed == 0) { return false; }
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
