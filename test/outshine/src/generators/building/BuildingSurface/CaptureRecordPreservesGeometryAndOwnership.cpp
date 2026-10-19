#include "BuildingScratch.h"
#include "BuildingSurfaceCapture.h"
#include "Check.h"
#include "Geodesy.h"
#include "StructureMesher.h"
#include <math/Vec3.h>
#include <scene/ProjectedErrorBudget.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

namespace {
template <typename T> bool SameBytes(const std::vector<T> &left, const std::vector<T> &right) {
  return std::ranges::equal(std::as_bytes(std::span(left)), std::as_bytes(std::span(right)));
}

bool SameMesh(const outshine::Raised &left, const outshine::Raised &right) {
  return SameBytes(left.WallCorners, right.WallCorners) &&
         SameBytes(left.RoofCorners, right.RoofCorners) && SameBytes(left.WallRun, right.WallRun) &&
         SameBytes(left.RoofRun, right.RoofRun) && SameBytes(left.WallColours, right.WallColours) &&
         SameBytes(left.WallMaterialM, right.WallMaterialM);
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using outshine::Test::Report;
  constexpr double kLatitude = 48;
  constexpr double kLongitude = 16;
  constexpr double kBaseM = 100;
  constexpr double kLatitudeSpan = 0.0001;
  constexpr double kLongitudeSpan = 0.00015;
  constexpr double kHeightM = 12;
  constexpr double kEyeDistanceM = 1000;
  constexpr double kEyeHeightM = 80;
  constexpr Vec3f kWallColour{{0.4f, 0.3f, 0.2f}};
  constexpr ProjectedErrorBudget projection{.FocalPx = 64, .AllowedErrorPx = 1};
  constexpr size_t kRectangleFaces = 6;
  constexpr std::array<double, 8> ring{kLatitude,
                                       kLongitude,
                                       kLatitude,
                                       kLongitude + kLongitudeSpan,
                                       kLatitude + kLatitudeSpan,
                                       kLongitude + kLongitudeSpan,
                                       kLatitude + kLatitudeSpan,
                                       kLongitude};
  Vec3 anchor;
  GeoToEcef({.LongitudeDeg = kLongitude, .LatitudeDeg = kLatitude, .HeightM = kBaseM}, anchor);
  const auto axes = EnuAxesEcef({.LongitudeDeg = kLongitude, .LatitudeDeg = kLatitude});
  std::array<StructurePlan, 2> plans;
  for (auto &plan : plans) {
    plan = {.RingLatLon = ring,
            .InnerRings = {},
            .RingPointsLatLon = {},
            .BaseAslM = kBaseM,
            .SeatAslM = kBaseM,
            .FootAslM = kBaseM,
            .CornerAslM = {},
            .HeightM = kHeightM,
            .HeightMeasured = true,
            .Street = {},
            .AnchorEcef = anchor,
            .Coarseness = LevelOfDetail::Shell,
            .RecessedOpenings = false,
            .PitchedShare = 0.0,
            .WallColour = kWallColour,
            .Facade = {}};
  }
  plans[0].Coarseness = LevelOfDetail::Fine;
  BuildingScratch scratch;
  const auto eye = axes.East * -kEyeDistanceM + axes.Up * kEyeHeightM;
  const auto ordinary = CaptureBuildingSurfaces(plans, eye, projection, scratch);
  BuildingSurfaceCaptureRecord record;
  const auto observed = CaptureBuildingSurfaces(plans, eye, projection, scratch, &record);
  CHECK(ordinary && observed, "recording captures the same supported native plans");
  if (!ordinary || !observed) { return Report(); }
  CHECK(SameMesh(ordinary->Mesh, observed->Mesh) && ordinary->EyeRadiusM == observed->EyeRadiusM,
        "recording changes no emitted vertex, index, material byte or movement bound");
  CHECK(record.FaceSizePx == 4 * static_cast<int>(projection.FocalPx) &&
            record.VisibleFaces.size() == plans.size() && record.Patches.size() == plans.size() &&
            record.PatchFallbacks.size() == plans.size() && record.Emissions.size() == plans.size(),
        "the actual cube resolution and every source slot remain explicit");
  CHECK(record.Emissions[0] == BuildingSurfaceEmission::FullMesh &&
            record.Emissions[1] != BuildingSurfaceEmission::FullMesh,
        "the record distinguishes native whole meshes from selected surface emission");
  CHECK(
      !observed->Mesh.WallColours.empty() &&
          std::ranges::any_of(record.Patches, [](const auto &patches) { return !patches.empty(); }),
      "the control exercises nonempty appearance and captured patch ownership");
  for (size_t source = 0; source < plans.size(); ++source) {
    CHECK(record.VisibleFaces[source].size() == kRectangleFaces,
          "visibility retains every native bottom, roof and wall face slot");
    for (const auto &patch : record.Patches[source]) {
      CHECK(patch.Sample.Part == 0 && patch.Sample.Face < kRectangleFaces,
            "capture patches preserve their actual source part and face");
      for (const auto &corner : patch.Corners) {
        CHECK(std::ranges::all_of(corner, [](double value) { return std::isfinite(value); }),
              "recorded patch corners retain finite native coordinates");
      }
    }
  }
  const auto invalid = CaptureBuildingSurfaces(plans, eye, {.FocalPx = 0}, scratch, &record);
  CHECK(!invalid && record.FaceSizePx == 0 && record.VisibleFaces.empty() &&
            record.Patches.empty() && record.PatchFallbacks.empty() && record.Emissions.empty(),
        "a failed request cannot expose a stale successful capture record");
  return Report();
}
