#include "src/generators/road/Corridors.h"
#include "src/generators/road/ProfiledRoadMesher.h"
#include "Check.h"
#include "EarthworkPress.h"
#include "src/generators/road/RoadMesher.h"
#include <math/Units.h>
#include <cmath>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

class RecordingMesher final : public outshine::RoadMesher {
public:
  explicit RecordingMesher(double gradient) : Gradient_(gradient) {}

  outshine::RoadMeshingStats Sweep(std::span<const outshine::RoadStation> along,
                                   outshine::RoadSweep how,
                                   outshine::RoadMeshBuffers &into) const override {
    double runM = 0;
    for (size_t at = 1; at < along.size(); ++at) {
      const double segmentM = std::hypot(along[at].EastM - along[at - 1].EastM,
                                         along[at].NorthM - along[at - 1].NorthM);
      runM += segmentM;
      MostRiseViolationM_ =
          std::max(MostRiseViolationM_,
                   std::abs(along[at].GradeM - along[at - 1].GradeM) - Gradient_ * segmentM);
    }
    if (how.Form == outshine::RibbonForm::Surface && std::abs(along.front().EastM) < 1e-6 &&
        along.front().NorthM > 1 && along.back().NorthM > along.front().NorthM) {
      ApproachRunM_ = runM;
    }
    return Native_.Sweep(along, how, into);
  }

  void Junction(std::span<const outshine::RoadGate> gates,
                outshine::RoadPlane plane,
                const outshine::Vec3f &colour,
                outshine::RoadMeshBuffers &into) const override {
    Native_.Junction(gates, plane, colour, into);
  }

  [[nodiscard]] double ApproachRunM() const { return ApproachRunM_; }

  [[nodiscard]] double MostRiseViolationM() const { return MostRiseViolationM_; }

private:
  outshine::Generators::ProfiledRoadMesher Native_;
  double Gradient_;
  mutable double ApproachRunM_ = 0;
  mutable double MostRiseViolationM_ = 0;
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
        "the approach uses production road and water rules");
  if (!vegetation.Ready()) { return Report(); }
  int water = -1;
  for (size_t row = 0; row < vegetation.TemplateCount(); ++row) {
    if (vegetation.Rows()[row].GroundClass == materials.Find("water")) {
      water = static_cast<int>(row);
      break;
    }
  }
  CHECK(water >= 0, "water is classified");
  if (water < 0) { return Report(); }
  const std::array<std::string, 1> layers{"streets"};
  Generators::Osm::OsmField vectors(14, layers);
  const std::array declared{
      Generators::Osm::OsmField::Declared{.Layer = "streets",
                                          .Key = "kind",
                                          .Value = "residential",
                                          .Bridge = true,
                                          .Level = 1,
                                          .LatLon = {49.0005, 8, 49.0015, 8}},
      Generators::Osm::OsmField::Declared{
          .Layer = "streets", .Key = "kind", .Value = "residential", .LatLon = {49, 8, 49.0005, 8}},
      Generators::Osm::OsmField::Declared{
          .Layer = "streets", .Key = "kind", .Value = "residential", .LatLon = {49, 8, 49, 8.0007}},
      Generators::Osm::OsmField::Declared{.Layer = "streets",
                                          .Key = "kind",
                                          .Value = "residential",
                                          .LatLon = {49, 8, 49, 7.9993}}};
  CHECK(vectors.Declare(declared, origin).has_value(),
        "a ground junction connects to a bridge approach");
  Generators::Osm::StreetField ways;
  CHECK(ways.Ingest(vectors, vegetation) == declared.size(), "all four roads are ingested");
  const TangentFrame standing = TangentFrame::At(origin);
  auto grid = std::make_shared<ClassStructure::Grid>();
  grid->W = grid->H = 1;
  grid->OrgE = grid->OrgN = -500;
  grid->CellM = 1000;
  grid->Cells = {static_cast<uint32_t>(water) | (1u << 8), 0};
  const std::shared_ptr<const ClassStructure> classes = std::make_shared<ClassStructure>(
      standing, grid, std::make_shared<const ClassStructure::Grid>(), ClassStructure::FromRun{});
  const TriangleBvh empty = TriangleBvh::Over({}, {});
  const Drape drape{.Surface = empty, .Field = [](EastNorth) { return std::optional<double>{0}; }};
  const Generators::Corridors::Site site{
      .Vectors = &vectors,
      .Ways = ways,
      .Materials = materials,
      .Vegetation = vegetation,
      .Standing = standing,
      .Draped = drape,
      .Classes = classes,
      .WaterUpM = [](LongitudeLatitude) { return std::optional<double>{10}; },
      .EyeLatDeg = origin.LatitudeDeg,
      .EyeLonDeg = origin.LongitudeDeg,
      .Projection = {.FocalPx = 800}};
  const RecordingMesher mesher(ways.Ways()[1].MaxGradient);
  const Generators::Corridors corridors(mesher);
  Geometry geometry;
  std::vector<EarthworkStamp> contacts;
  std::vector<DiagnosticSample> notes;
  CHECK(corridors.Lay(site, geometry, contacts, notes),
        "the approach generates native road contacts");
  const std::array<EastNorth, 4> samples{{{0, 0}, {0, 1}, {1, 0}, {-1, 0}}};
  std::array<double, 4> heights{};
  (void)ApplyEarthworkStamps(contacts, samples, heights, kMostEarthworkM);
  const auto [low, high] = std::minmax_element(heights.begin(), heights.end());
  const double bridgeRunM = (declared[0].LatLon[2] - declared[0].LatLon[0]) * kMPerDegLat;
  double clearanceM = 0;
  for (const auto &band : vegetation.WaterBands()) {
    clearanceM = band.ClearanceM;
    if (bridgeRunM <= band.RunM) { break; }
  }
  const double approachRunM = mesher.ApproachRunM();
  CHECK(approachRunM > 0, "the constructed approach excludes the flat junction footprint");
  CHECK(mesher.MostRiseViolationM() < 1e-8,
        "all constructed bridge and approach segments obey their permitted gradients");
  const double expectedM =
      10 + clearanceM - ways.Ways()[1].MaxGradient * approachRunM - kPavementLipM;
  CHECK(std::abs(heights[0] - expectedM) < 0.01 && *high - *low < 0.01,
        "the ground junction and its arms share the propagated approach height instead of DEM "
        "height");
  return Report();
}
