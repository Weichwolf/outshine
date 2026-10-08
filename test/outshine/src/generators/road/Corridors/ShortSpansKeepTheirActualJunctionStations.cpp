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

class ObservedMesher final : public outshine::RoadMesher {
public:
  mutable std::vector<outshine::RoadGate> Gates;
  outshine::Generators::ProfiledRoadMesher Native;

  outshine::RoadMeshingStats Sweep(std::span<const outshine::RoadStation> along,
                                   outshine::RoadSweep how,
                                   outshine::RoadMeshBuffers &into) const override {
    return Native.Sweep(along, how, into);
  }

  void Junction(std::span<const outshine::RoadGate> gates,
                outshine::RoadPlane plane,
                const outshine::Vec3f &wears,
                outshine::RoadMeshBuffers &into) const override {
    Gates.insert(Gates.end(), gates.begin(), gates.end());
    Native.Junction(gates, plane, wears, into);
  }
};

}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr LongitudeLatitude origin{.LongitudeDeg = 8, .LatitudeDeg = 49};
  Ground::GroundMaterials materials;
  Ground::VegetationTemplates vegetation;
  CHECK(materials.Load("src/assets/world/ground-materials.json") &&
            vegetation.Load("src/assets/world/vegetation.json", materials),
        "short spans use production road rules");
  if (!vegetation.Ready()) { return Report(); }
  const std::array<std::string, 1> layers{"streets"};
  const TangentFrame frame = TangentFrame::At(origin);
  const TriangleBvh empty = TriangleBvh::Over({}, {});
  const auto grade = [](double e, double n) { return .02 * e + .04 * n; };
  const Drape drape{.Surface = empty, .Field = [&](EastNorth at) -> std::optional<double> {
                      return grade(at.EastM, at.NorthM);
                    }};
  const std::shared_ptr<const ClassStructure> noClasses;
  const ObservedMesher mesher;
  const Generators::Corridors corridors(mesher);
  for (const double separation : {.000012, .000054, .00018}) {
    for (const bool reverse : {false, true}) {
      const double north = origin.LatitudeDeg + separation;
      const double skew = separation > .0001 ? .000002 : 0;
      Generators::Osm::OsmField vectors(14, layers);
      std::array declared{Generators::Osm::OsmField::Declared{.Layer = "streets",
                                                              .Key = "kind",
                                                              .Value = "residential",
                                                              .LatLon = {49, 8, north, 8}},
                          Generators::Osm::OsmField::Declared{.Layer = "streets",
                                                              .Key = "kind",
                                                              .Value = "residential",
                                                              .LatLon = {49 + skew, 8, 49, 7.999}},
                          Generators::Osm::OsmField::Declared{.Layer = "streets",
                                                              .Key = "kind",
                                                              .Value = "residential",
                                                              .LatLon = {49 - skew, 8, 49, 8.001}},
                          Generators::Osm::OsmField::Declared{.Layer = "streets",
                                                              .Key = "kind",
                                                              .Value = "residential",
                                                              .LatLon = {north, 8, north, 7.999}},
                          Generators::Osm::OsmField::Declared{.Layer = "streets",
                                                              .Key = "kind",
                                                              .Value = "residential",
                                                              .LatLon = {north, 8, north, 8.001}}};
      if (reverse) {
        for (auto &road : declared) {
          std::swap(road.LatLon[0], road.LatLon[2]);
          std::swap(road.LatLon[1], road.LatLon[3]);
        }
      }
      CHECK(vectors.Declare(declared, origin).has_value(), "nearby junction inputs are valid");
      Generators::Osm::StreetField ways;
      CHECK(ways.Ingest(vectors, vegetation) == declared.size(), "all five roads are ingested");
      const Generators::Corridors::Site site{.Vectors = &vectors,
                                             .Ways = ways,
                                             .Materials = materials,
                                             .Vegetation = vegetation,
                                             .Standing = frame,
                                             .Draped = drape,
                                             .Classes = noClasses,
                                             .EyeLatDeg = origin.LatitudeDeg,
                                             .EyeLonDeg = origin.LongitudeDeg,
                                             .Projection = {.FocalPx = 800}};
      mesher.Gates.clear();
      Geometry geometry;
      std::vector<EarthworkStamp> contacts;
      std::vector<DiagnosticSample> notes;
      CHECK(corridors.Lay(site, geometry, contacts, notes) && contacts.size() >= 2 &&
                mesher.Gates.size() == 6,
            "both three-arm junctions produce native gates and contacts");
      double fromN = 0;
      double toN = 0;
      for (const auto &gate : mesher.Gates) {
        if (gate.OutN > .9) { fromN = gate.NorthM; }
        if (gate.OutN < -.9) { toN = gate.NorthM; }
      }
      const double lengthM = frame.ToLocalGroundPosition({8, north}).NorthM;
      CHECK(toN - fromN >= std::min(2.0, .5 * lengthM) - .00001,
            "opposing junction cuts leave a positive connecting span even on a sub-two-metre road");
      double mostDriftM = 0;
      for (size_t at = 2; at < contacts.size(); ++at) {
        const auto &contact = contacts[at];
        mostDriftM =
            std::max(mostDriftM, std::abs(contact.PlateauM - grade(contact.AtE, contact.AtN)));
      }
      CHECK(mostDriftM < .00001,
            "short spans and quantized node offsets preserve their actual analytic cut heights");
    }
  }
  return Report();
}
