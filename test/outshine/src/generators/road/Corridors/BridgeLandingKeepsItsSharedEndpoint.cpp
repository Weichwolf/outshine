#include "Check.h"
#include "Corridors.h"
#include "ProfiledRoadMesher.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

struct RecordingMesher final : outshine::RoadMesher {
  double CrossingEastM = 0;
  mutable double HighestM = 0;
  mutable std::array<double, 2> CrossingHeightM{};
  mutable std::array<bool, 2> CrossingFound{};
  outshine::Generators::ProfiledRoadMesher Native;

  outshine::RoadMeshingStats Sweep(std::span<const outshine::RoadStation> along,
                                   outshine::RoadSweep how,
                                   outshine::RoadMeshBuffers &into) const override {
    for (const auto &station : along) { HighestM = std::max(HighestM, station.GradeM); }
    const size_t deck = how.Form == outshine::RibbonForm::ClosedShell ? 1 : 0;
    for (size_t at = 1; at < along.size(); ++at) {
      const auto &a = along[at - 1];
      const auto &b = along[at];
      if (deck == 1) {
        if ((a.EastM - CrossingEastM) * (b.EastM - CrossingEastM) > 0 || a.EastM == b.EastM) {
          continue;
        }
        CrossingHeightM[deck] =
            std::lerp(a.GradeM, b.GradeM, (CrossingEastM - a.EastM) / (b.EastM - a.EastM));
      } else {
        if (a.NorthM * b.NorthM > 0 || a.NorthM == b.NorthM) { continue; }
        CrossingHeightM[deck] = std::lerp(a.GradeM, b.GradeM, -a.NorthM / (b.NorthM - a.NorthM));
      }
      CrossingFound[deck] = true;
    }
    return Native.Sweep(along, how, into);
  }

  void Junction(std::span<const outshine::RoadGate> gates,
                outshine::RoadPlane plane,
                const outshine::Vec3f &wears,
                outshine::RoadMeshBuffers &into) const override {
    Native.Junction(gates, plane, wears, into);
  }
};

void CheckLanding(bool joined, bool reversed) {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr LongitudeLatitude origin{.LongitudeDeg = 8, .LatitudeDeg = 49};
  const auto frame = TangentFrame::At(origin);
  Ground::GroundMaterials materials;
  Ground::VegetationTemplates vegetation;
  CHECK(materials.Load("src/assets/world/ground-materials.json") &&
            vegetation.Load("src/assets/world/vegetation.json", materials),
        "landing dimensions use production recipes");
  if (!vegetation.Ready()) { return; }
  const std::array<std::string, 1> layers{"streets"};
  Generators::Osm::OsmField vectors(14, layers);
  std::array declared{
      Generators::Osm::OsmField::Declared{
          .Layer = "streets", .Key = "kind", .Value = "footway", .Bridge = true, .Level = 1},
      Generators::Osm::OsmField::Declared{.Layer = "streets", .Key = "kind", .Value = "footway"}};
  const auto append = [&](size_t way, EastNorth point) {
    const auto at = frame.ApproximateGeographicAt(point);
    declared[way].LatLon.insert(declared[way].LatLon.end(), {at.LatitudeDeg, at.LongitudeDeg});
  };
  append(0, {-20, 0});
  append(0, {0, 0});
  append(1, {joined ? 0 : .3, 0});
  append(1, {-2, 1});
  append(1, {-2, -1});
  append(1, {-20, -1});
  if (reversed) {
    for (auto &way : declared) {
      for (size_t at = 0; at < way.LatLon.size() / 4; ++at) {
        const size_t other = way.LatLon.size() - 2 * at - 2;
        std::swap(way.LatLon[2 * at], way.LatLon[other]);
        std::swap(way.LatLon[2 * at + 1], way.LatLon[other + 1]);
      }
    }
  }
  CHECK(vectors.Declare(declared, origin).has_value(), "both landing axes are supplied");
  Generators::Osm::StreetField ways;
  CHECK(ways.Ingest(vectors, vegetation) == declared.size(), "both paths retain their levels");
  auto network = Path::Network::Create({.CellM = 32}, {});
  CHECK(network.has_value(), "the 2D network is available");
  if (!network) { return; }
  for (size_t at = 0; at < ways.Ways().size(); ++at) {
    const auto &way = ways.Ways()[at];
    CHECK(network
              ->Lay(vectors.Points().subspan(2u * way.FirstPoint, 2u * way.PointCount), {.Tag = at})
              .has_value(),
          "both path axes enter the production crossing plan");
  }
  const TriangleBvh empty = TriangleBvh::Over({}, {});
  const Drape drape{.Surface = empty, .Field = [](EastNorth) { return std::optional<double>{0}; }};
  const std::shared_ptr<const ClassStructure> noClasses;
  RecordingMesher mesher;
  mesher.CrossingEastM = frame
                             .ToLocalGroundPosition({.LongitudeDeg = declared[1].LatLon[3],
                                                     .LatitudeDeg = origin.LatitudeDeg})
                             .EastM;
  const Generators::Corridors corridors(mesher);
  const Generators::Corridors::Site site{.Vectors = &vectors,
                                         .Ways = ways,
                                         .Materials = materials,
                                         .Vegetation = vegetation,
                                         .Network = &*network,
                                         .Standing = frame,
                                         .Draped = drape,
                                         .Classes = noClasses,
                                         .EyeLatDeg = origin.LatitudeDeg,
                                         .EyeLonDeg = origin.LongitudeDeg,
                                         .Projection = {.FocalPx = 800}};
  Geometry geometry;
  std::vector<EarthworkStamp> contacts;
  std::vector<DiagnosticSample> notes;
  CHECK(corridors.Lay(site, geometry, contacts, notes) && geometry.parts() != 0,
        "the landing produces an admissible native profile and mesh");
  if (joined) {
    CHECK(mesher.HighestM < .5,
          "an intersection inside the shared landing cannot invent an underpass");
  } else {
    CHECK(mesher.CrossingFound[0] && mesher.CrossingFound[1],
          "both separated paths retain their crossing stations");
    const auto claim =
        "nearby paths retain their clearance: lower=" + std::to_string(mesher.CrossingHeightM[0]) +
        " upper=" + std::to_string(mesher.CrossingHeightM[1]) +
        " required=" + std::to_string(ways.Ways()[1].ClearanceM + kSealedDepthM);
    CHECK(mesher.CrossingHeightM[1] - kSealedDepthM >=
              mesher.CrossingHeightM[0] + ways.Ways()[1].ClearanceM - 1e-6,
          claim.c_str());
  }
}

}

int main() {
  for (const bool joined : {false, true}) {
    for (const bool reversed : {false, true}) { CheckLanding(joined, reversed); }
  }
  return outshine::Test::Report();
}
