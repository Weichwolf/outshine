#include "Corridors.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
constexpr size_t kSlicesMost = 2048;
constexpr double kFarLongitude = 18;
constexpr double kUnusedEastM = 100;
constexpr double kTerrainSlope = 0.002;
constexpr double kSouthLatitude = 48.9998;
constexpr double kNorthLatitude = 49.0002;
constexpr double kUnusedLongitude = 8.01;
constexpr double kFocalPx = 800;
constexpr LongitudeLatitude kOrigin{.LongitudeDeg = 8,
                                    .LatitudeDeg = site.Standing.Anchor().LatitudeDeg};

class MeshObserver final : public RoadMesher {
public:
  mutable size_t Calls = 0;

  RoadMeshingStats
  Sweep(std::span<const RoadStation>, RoadSweep, RoadMeshBuffers &) const override {
    ++Calls;
    return {};
  }

  void
  Junction(std::span<const RoadGate>, RoadPlane, const Vec3f &, RoadMeshBuffers &) const override {
    ++Calls;
  }
};

struct Product {
  RoadPageRecipe Recipe;
  std::vector<EarthworkStamp> Contacts;
};

std::optional<Product>
Plan(const Corridors &generator, const Corridors::Site &site, std::span<const uint32_t> selected) {
  auto started = Corridors::BeginFunctionalPlan(site, selected);
  if (!started) { return {}; }
  auto &job = **started;
  bool complete = false;
  for (size_t at = 0; at < kSlicesMost && !complete; ++at) {
    const auto ready = generator.AdvancePlan(job, site, 1, 1);
    if (!ready) { return {}; }
    complete = *ready;
  }
  if (!complete) { return {}; }
  const auto parts = Corridors::PlannedParts(job);
  if (!parts) { return {}; }
  auto recipe = Corridors::PageRecipe(job, site, *parts);
  auto contacts = Corridors::TakeTerrainContacts(job);
  if (!recipe || !contacts) { return {}; }
  return Product{.Recipe = std::move(*recipe), .Contacts = std::move(*contacts)};
}

bool SameProfile(const Product &a, const Product &b) {
  if (a.Recipe.Intervals.size() != 1 || b.Recipe.Intervals.size() != 1) { return false; }
  const auto &first = a.Recipe.Intervals.front();
  const auto &second = b.Recipe.Intervals.front();
  const auto station = [](const RoadStation &at) {
    return std::tie(at.EastM, at.NorthM, at.GradeM, at.Node);
  };
  return first.Lane == second.Lane && first.Surface.Profile == second.Surface.Profile &&
         first.Surface.HalfWidthM == second.Surface.HalfWidthM &&
         std::ranges::equal(first.Stations, second.Stations, {}, station, station) &&
         a.Contacts == b.Contacts;
}

void CheckContext(const Corridors &generator, const Corridors::Site &site) {
  using namespace outshine::Test;
  const std::array<uint32_t, 1> selected{0};
  auto started = Corridors::BeginFunctionalPlan(site, selected);
  CHECK(started.has_value(), "functional demand binds a source snapshot");
  if (!started) { return; }
  const auto changedFrame = TangentFrame::At(
      {.LongitudeDeg = kFarLongitude, .LatitudeDeg = site.Standing.Anchor().LatitudeDeg});
  const Corridors::Site reframed{.Vectors = site.Vectors,
                                 .Ways = site.Ways,
                                 .Materials = site.Materials,
                                 .Vegetation = site.Vegetation,
                                 .Standing = changedFrame,
                                 .Draped = site.Draped,
                                 .Classes = site.Classes};
  CHECK(!generator.AdvancePlan(**started, reframed, 1, 1),
        "a different geometry anchor cannot mix coordinate frames in an active plan");
  const std::array<uint32_t, 1> missing{static_cast<uint32_t>(site.Ways.Ways().size())};
  CHECK(!Corridors::BeginFunctionalPlan(site, missing), "absent source ways cannot be requested");
}
}

int main() {
  using namespace outshine::Test;
  constexpr LongitudeLatitude origin{.LongitudeDeg = 8,
                                     .LatitudeDeg = site.Standing.Anchor().LatitudeDeg};
  Ground::GroundMaterials materials;
  Ground::VegetationTemplates recipes;
  CHECK(materials.Load("src/assets/world/ground-materials.json") &&
            recipes.Load("src/assets/world/vegetation.json", materials),
        "production recipes load");
  if (!recipes.Ready()) { return Report(); }
  const std::array<std::string, 1> layers{"streets"};
  Osm::OsmField field(14, layers);
  const std::array<Osm::OsmField::Declared, 2> roads{
      {{.Layer = "streets",
        .Key = "kind",
        .Value = "residential",
        .LatLon = {kSouthLatitude, origin.LongitudeDeg, kNorthLatitude, origin.LongitudeDeg}},
       {.Layer = "streets",
        .Key = "kind",
        .Value = "residential",
        .LatLon = {kSouthLatitude, kUnusedLongitude, kNorthLatitude, kUnusedLongitude}}}};
  CHECK(field.Declare(roads, origin).has_value(), "two independent source roads are present");
  Osm::StreetField ways;
  CHECK(ways.Ingest(field, recipes) == 2, "both source ways reach normalization");
  const TangentFrame frame = TangentFrame::At(origin);
  const TriangleBvh surface = TriangleBvh::Over({}, {});
  size_t samples = 0;
  size_t unusedSamples = 0;
  const Drape drape{.Surface = surface, .Field = [&](EastNorth at) -> std::optional<double> {
                      ++samples;
                      unusedSamples += static_cast<size_t>(at.EastM > kUnusedEastM);
                      return 10 + kTerrainSlope * at.NorthM;
                    }};
  const std::shared_ptr<const TerrainClassificationMap> classes;
  const Corridors::Site near{.Vectors = &field,
                             .Ways = ways,
                             .Materials = materials,
                             .Vegetation = recipes,
                             .Standing = frame,
                             .Draped = drape,
                             .Classes = classes,
                             .EyeLatDeg = origin.LatitudeDeg,
                             .EyeLonDeg = origin.LongitudeDeg,
                             .Projection = {.FocalPx = kFocalPx}};
  auto far = near;
  far.EyeLonDeg = kFarLongitude;
  MeshObserver observer;
  const Corridors generator(observer);
  const auto first = Plan(generator, near, std::array<uint32_t, 2>{0, 0});
  const size_t firstSamples = samples;
  const auto second = Plan(generator, far, std::array<uint32_t, 1>{0});
  CHECK(first && second && SameProfile(*first, *second),
        "same geographic demand has identical functional profiles and contacts at either camera");
  CHECK(first && first->Recipe.Intervals.size() == 1 && first->Recipe.Intervals.front().Lane == 0 &&
            !first->Contacts.empty() && observer.Calls == 0,
        "only the requested way is planned without any fine sweep or junction mesh");
  CHECK(firstSamples > 0 && samples == firstSamples * 2 && unusedSamples == 0,
        "duplicate requests coalesce and unrelated ways trigger no height queries");
  const size_t beforeEmpty = samples;
  const auto empty = Plan(generator, near, {});
  CHECK(empty && empty->Recipe.Intervals.empty() && empty->Recipe.Junctions.empty() &&
            empty->Contacts.empty() && samples == beforeEmpty,
        "empty functional demand performs no height or geometry work");
  CheckContext(generator, near);
  std::println("FUNCTIONAL_PLAN source_ways={} selected_ways=1 samples_per_plan={} "
               "unused_samples={} mesh_calls={}",
               ways.Ways().size(),
               firstSamples,
               unusedSamples,
               observer.Calls);
  return Report();
}
