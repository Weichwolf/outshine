#include "Check.h"
#include "Corridors.h"
#include "ProfiledRoadMesher.h"
#include <scene/SurfaceClearance.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace {

struct SelectedSurface {
  std::vector<outshine::Vec3> Vertices;
  std::vector<std::array<uint32_t, 3>> Triangles;

  void
  Append(const outshine::RoadMeshBuffers &mesh, std::span<const uint32_t> indices, double facing) {
    for (size_t at = 0; at < indices.size(); at += 3) {
      const std::array face{indices[at], indices[at + 1], indices[at + 2]};
      if (!std::ranges::all_of(
              face, [&](uint32_t vertex) { return facing * mesh.NormalM[vertex * 3 + 1] > .5; })) {
        continue;
      }
      const auto first = static_cast<uint32_t>(Vertices.size());
      for (uint32_t vertex : face) {
        Vertices.push_back({{mesh.PositionM[vertex * 3],
                             -mesh.PositionM[vertex * 3 + 2],
                             mesh.PositionM[vertex * 3 + 1]}});
      }
      Triangles.push_back({first, first + 1, first + 2});
    }
  }

  outshine::HeightSurfaceView View() const { return {Vertices, Triangles}; }
};

struct RecordingMesher final : outshine::RoadMesher {
  mutable std::array<double, 2> CrossingHeightM{};
  mutable std::array<SelectedSurface, 2> Top, Underside;
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
    const size_t before = into.Index.size();
    const auto result = Native.Sweep(along, how, into);
    const auto faces = std::span(into.Index).subspan(before);
    Top[eastward ? 1 : 0].Append(into, faces, 1);
    Underside[eastward ? 1 : 0].Append(into, faces, -1);
    return result;
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
  SameBridgeTier,
  DenseStack,
  ObliqueStack
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
  const auto gap = MeasureSurfaceClearance(mesher.Top[0].View(), mesher.Underside[1].View());
  CHECK(gap.has_value(), "selected native road top and bridge underside overlap over their width");
  if (gap) {
    if (gap->MinimumM < lower.ClearanceM - 1e-4) {
      std::printf("crossing=%u gap=%.9f required=%.9f xy=%.9f,%.9f centre=%.9f,%.9f\n",
                  static_cast<unsigned>(kind),
                  gap->MinimumM,
                  lower.ClearanceM,
                  gap->Witness[0],
                  gap->Witness[1],
                  mesher.CrossingHeightM[0],
                  mesher.CrossingHeightM[1]);
    }
    CHECK(gap->MinimumM >= lower.ClearanceM - 1e-4,
          "the entire rendered overlap preserves vehicle clearance within float32 mesh precision");
  }
}

std::vector<outshine::Generators::Osm::OsmField::Declared> DeclaredCrossing(Crossing kind,
                                                                            bool duplicate) {
  using namespace outshine;
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
  if (kind == Crossing::ObliqueStack) { declared[1].LatLon = {48.999, 7.998, 49.001, 8.002}; }
  if (kind == Crossing::DenseStack) {
    for (auto &source : declared) {
      const auto endpoints = source.LatLon;
      source.LatLon.clear();
      for (size_t at = 0; at <= 255; ++at) {
        const double weight = static_cast<double>(at) / 255;
        source.LatLon.push_back(std::lerp(endpoints[0], endpoints[2], weight));
        source.LatLon.push_back(std::lerp(endpoints[1], endpoints[3], weight));
      }
    }
  }
  return declared;
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
  auto declared = DeclaredCrossing(kind, duplicate);
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
  for (const auto kind : {Crossing::DenseStack, Crossing::ObliqueStack}) {
    CheckStack(false, false, kind);
    CheckStack(true, false, kind);
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
