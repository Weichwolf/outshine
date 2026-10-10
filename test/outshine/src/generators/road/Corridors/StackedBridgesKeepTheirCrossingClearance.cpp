#include "Check.h"
#include "Corridors.h"
#include "ProfiledRoadMesher.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {

struct RecordingMesher final : outshine::RoadMesher {
  mutable std::array<double, 2> CrossingHeightM{};
  mutable size_t JunctionCount = 0;
  outshine::Generators::ProfiledRoadMesher Native;

  outshine::RoadMeshingStats Sweep(std::span<const outshine::RoadStation> along,
                                   outshine::RoadSweep how,
                                   outshine::RoadMeshBuffers &into) const override {
    const bool eastward = std::abs(along.back().EastM - along.front().EastM) >
                          std::abs(along.back().NorthM - along.front().NorthM);
    for (size_t at = 1; at < along.size(); ++at) {
      const double first = eastward ? along[at - 1].EastM : along[at - 1].NorthM;
      const double last = eastward ? along[at].EastM : along[at].NorthM;
      if (first * last > 0 || first == last) { continue; }
      CrossingHeightM[eastward ? 1 : 0] =
          std::lerp(along[at - 1].GradeM, along[at].GradeM, -first / (last - first));
    }
    return Native.Sweep(along, how, into);
  }

  void Junction(std::span<const outshine::RoadGate> gates,
                outshine::RoadPlane plane,
                const outshine::Vec3f &wears,
                outshine::RoadMeshBuffers &into) const override {
    Native.Junction(gates, plane, wears, into);
    ++JunctionCount;
  }
};

enum class Crossing {
  Stack,
  NearEnd,
  GroundEnds,
  RoadMeetsPath,
  RailMeetsPath,
  RailEndMeetsPath,
  SameBridgeTier
};

void CheckHeights(Crossing kind,
                  const RecordingMesher &mesher,
                  const outshine::Generators::Osm::StreetField::Way &lower) {
  using namespace outshine;
  using namespace outshine::Test;
  if (kind == Crossing::SameBridgeTier) {
    CHECK(std::abs(mesher.CrossingHeightM[0] - mesher.CrossingHeightM[1]) < 1e-6,
          "road and rail share one deck elevation without joining their traffic networks");
    return;
  }
  if (lower.Bridge) {
    CHECK(mesher.CrossingHeightM[0] >= kSealedDepthM - 1e-6,
          "clearance planning does not excavate the lower bridge into its ground support");
  }
  CHECK(mesher.CrossingHeightM[1] - kSealedDepthM >=
            mesher.CrossingHeightM[0] + lower.ClearanceM - 1e-6,
        "the upper underside clears the lower road envelope, independently of source order");
}

void CheckStack(bool reversed, bool duplicate, Crossing kind = Crossing::Stack) {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr LongitudeLatitude origin{.LongitudeDeg = 8, .LatitudeDeg = 49};
  Ground::GroundMaterials materials;
  Ground::VegetationTemplates vegetation;
  CHECK(materials.Load("src/assets/world/ground-materials.json") &&
            vegetation.Load("src/assets/world/vegetation.json", materials),
        "both bridge decks use production road dimensions");
  if (!vegetation.Ready()) { return; }
  const std::array<std::string, 1> layers{"streets"};
  Generators::Osm::OsmField vectors(14, layers);
  std::vector declared{Generators::Osm::OsmField::Declared{.Layer = "streets",
                                                           .Key = "kind",
                                                           .Value = "residential",
                                                           .Bridge = true,
                                                           .Level = 1,
                                                           .LatLon = {48.998, 8, 49.002, 8}},
                       Generators::Osm::OsmField::Declared{.Layer = "streets",
                                                           .Key = "kind",
                                                           .Value = "residential",
                                                           .Bridge = true,
                                                           .Level = 2,
                                                           .LatLon = {49, 7.996, 49, 8.004}}};
  if (kind == Crossing::NearEnd) {
    declared[0].Bridge = false;
    declared[0].Level = 0;
    declared[1].LatLon.back() = 8.00000065;
  }
  if (kind == Crossing::SameBridgeTier) {
    declared[0].Value = "rail";
    declared[1].Level = declared[0].Level;
  }
  if (kind == Crossing::GroundEnds || kind == Crossing::RoadMeetsPath ||
      kind == Crossing::RailEndMeetsPath) {
    declared[0].Bridge = false;
    declared[0].LatLon = {48.998, 8, 49, 8};
    declared[1].Bridge = false;
    declared[1].LatLon = {49, 7.996, 49, 8};
    declared.push_back(declared.front());
    declared.back().LatLon = {49, 8, 49.002, 8};
  }
  if (kind == Crossing::RoadMeetsPath) {
    declared[0].Value = "cycleway";
    declared[2].Value = "cycleway";
    declared[1].Value = "trunk";
    declared[1].Bridge = true;
  }
  if (kind == Crossing::RailMeetsPath || kind == Crossing::RailEndMeetsPath) {
    for (auto &line : declared) {
      line.Bridge = false;
      line.Level = 0;
    }
    declared[0].Value = "footway";
    declared[1].Value = "rail";
    if (declared.size() == 3) { declared[2].Value = "footway"; }
  }
  if (duplicate) {
    const auto copies = declared;
    declared.insert(declared.end(), copies.begin(), copies.end());
  }
  if (reversed) { std::ranges::reverse(declared); }
  CHECK(vectors.Declare(declared, origin).has_value(), "crossing decks declare distinct levels");
  Generators::Osm::StreetField ways;
  CHECK(ways.Ingest(vectors, vegetation) == declared.size(),
        "all supplied bridge lines are retained");
  auto network = Path::Network::Create({.CellM = 32}, {});
  CHECK(network.has_value(), "the 2D network is available");
  if (!network) { return; }
  for (size_t at = 0; at < ways.Ways().size(); ++at) {
    const auto &way = ways.Ways()[at];
    CHECK(network
              ->Lay(vectors.Points().subspan(2u * way.FirstPoint, 2u * way.PointCount), {.Tag = at})
              .has_value(),
          "each axis keeps its source association");
  }
  const TangentFrame standing = TangentFrame::At(origin);
  const TriangleBvh empty = TriangleBvh::Over({}, {});
  const Drape drape{.Surface = empty, .Field = [](EastNorth) { return std::optional<double>{0}; }};
  const std::shared_ptr<const ClassStructure> noClasses;
  const RecordingMesher mesher;
  const Generators::Corridors corridors(mesher);
  const Generators::Corridors::Site site{.Vectors = &vectors,
                                         .Ways = ways,
                                         .Materials = materials,
                                         .Vegetation = vegetation,
                                         .Network = &*network,
                                         .Standing = standing,
                                         .Draped = drape,
                                         .Classes = noClasses,
                                         .EyeLatDeg = origin.LatitudeDeg,
                                         .EyeLonDeg = origin.LongitudeDeg,
                                         .Projection = {.FocalPx = 800}};
  Geometry geometry;
  std::vector<EarthworkStamp> contacts;
  std::vector<DiagnosticSample> notes;
  CHECK(corridors.Lay(site, geometry, contacts, notes), "both deck products are generated");
  CHECK(mesher.JunctionCount == 0,
        "duplicate lines do not manufacture junctions or consume the available profile length");
  if (kind == Crossing::GroundEnds || kind == Crossing::RoadMeetsPath ||
      kind == Crossing::RailMeetsPath || kind == Crossing::RailEndMeetsPath) {
    return;
  }
  const auto &lower = ways.Ways()[reversed ? 1 : 0];
  CheckHeights(kind, mesher, lower);
}

}

int main() {
  for (const bool duplicate : {false, true}) {
    CheckStack(false, duplicate);
    CheckStack(true, duplicate);
    CheckStack(false, duplicate, Crossing::SameBridgeTier);
    CheckStack(true, duplicate, Crossing::SameBridgeTier);
  }
  CheckStack(false, false, Crossing::NearEnd);
  CheckStack(true, false, Crossing::NearEnd);
  CheckStack(false, false, Crossing::GroundEnds);
  CheckStack(true, false, Crossing::GroundEnds);
  for (const auto kind :
       {Crossing::RoadMeetsPath, Crossing::RailMeetsPath, Crossing::RailEndMeetsPath}) {
    CheckStack(false, false, kind);
    CheckStack(true, false, kind);
  }
  return outshine::Test::Report();
}
