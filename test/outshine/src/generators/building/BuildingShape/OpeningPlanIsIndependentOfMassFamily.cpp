#include "BuildingPreparation.h"
#include "BuildingScratch.h"
#include "BuildingShape.h"
#include "BuildingMesh.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace {
std::array<double, 8> Rectangle(double lengthM, double widthM) {
  constexpr double latitude = 48.23;
  constexpr double longitude = 16.41;
  constexpr double metresPerDegree = 111320.0;
  const double east = lengthM / (metresPerDegree * std::cos(latitude * std::numbers::pi / 180.0));
  const double north = widthM / metresPerDegree;
  return {latitude,
          longitude,
          latitude,
          longitude + east,
          latitude + north,
          longitude + east,
          latitude + north,
          longitude};
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  BuildingScratch scratch;
  BuildingMesh mesher;
  for (const auto &size : {std::array{80.0, 20.0, 24.0}, std::array{20.0, 14.0, 65.0}}) {
    const auto ring = Rectangle(size[0], size[1]);
    StructurePlan plan;
    plan.RingLatLon = ring;
    plan.HeightM = size[2];
    plan.HeightMeasured = true;
    plan.PitchedShare = 0;
    plan.Coarseness = LevelOfDetail::Shell;
    const auto unknown = PrepareBuildingShapes(plan, scratch);
    CHECK(unknown && unknown->size() == 1, "complete mass receives one independent opening plan");
    if (!unknown || unknown->empty()) { continue; }
    const auto form = unknown->front().Form;
    const auto top = unknown->front().TopM();
    CHECK((form == BuildingForm::Hall || form == BuildingForm::Tower) &&
              unknown->front().OpeningStyle == FacadeStyle::Block &&
              unknown->front().FloorM >= 2.5 && unknown->front().FloorM <= 3.5,
          "mass proportions do not turn an unknown occupied building into a closed service shaft");
    Raised opened;
    CHECK(mesher.Mesh(plan, scratch, opened).has_value(), "regular shell builds");
    plan.Facade = FacadeStyle::Tower;
    const auto closed = PrepareBuildingShapes(plan, scratch);
    CHECK(closed && closed->size() == 1 && closed->front().Form == form &&
              closed->front().OpeningStyle == FacadeStyle::Tower && closed->front().TopM() == top,
          "known service use changes openings without changing supplied height or silhouette");
    Raised sealed;
    CHECK(mesher.Mesh(plan, scratch, sealed).has_value() &&
              opened.WallRun.size() == sealed.WallRun.size() &&
              opened.RoofRun.size() == sealed.RoofRun.size(),
          "distant openings consume no additional shell triangles");
  }
  const auto shaftRing = Rectangle(6, 6);
  StructurePlan shaft;
  shaft.RingLatLon = shaftRing;
  shaft.HeightM = 80;
  shaft.PitchedShare = 0;
  const auto unknownShaft = PrepareBuildingShapes(shaft, scratch);
  CHECK(unknownShaft && unknownShaft->size() == 1 &&
            unknownShaft->front().OpeningStyle == FacadeStyle::Tower,
        "an unclassified narrow shaft is not assumed to be occupied housing");
  shaft.Facade = FacadeStyle::Block;
  const auto declaredHousing = PrepareBuildingShapes(shaft, scratch);
  CHECK(declaredHousing && declaredHousing->size() == 1 &&
            declaredHousing->front().OpeningStyle == FacadeStyle::Block,
        "declared housing use overrides conservative shaft fallback");
  const auto ring = Rectangle(80, 20);
  StructurePlan roofOnly;
  roofOnly.RingLatLon = ring;
  roofOnly.HeightM = 20.1;
  roofOnly.MinimumHeightM = 20;
  roofOnly.PitchedShare = 0;
  const auto parts = PrepareBuildingShapes(roofOnly, scratch);
  CHECK(parts && std::ranges::all_of(*parts,
                                     [](const BuildingShape &part) {
                                       return part.FloorM > 0 && std::isfinite(part.FloorM);
                                     }),
        "a thin raised roof retains valid opening coordinates");
  return Report();
}
