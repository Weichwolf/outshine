#include "BuildingMesh.h"
#include "StructureSurfaceRefinement.h"
#include "Geodesy.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const std::array<double, 8> ring{47, 9, 47, 9.00015, 47.0001, 9.00015, 47.0001, 9};
  StructurePlan plan;
  plan.RingLatLon = ring;
  plan.HeightM = 9;
  plan.HeightMeasured = true;
  GeoToEcef({.LongitudeDeg = 9, .LatitudeDeg = 47}, plan.AnchorEcef);
  const Vec3 localAnchor = plan.AnchorEcef;
  BuildingMesh mesher;
  auto scratch = mesher.Scratch();
  CHECK(!mesher.ShellSurfaceErrorM({}), "missing geometry cannot publish a shell bound");
  StoredVertex invalid;
  invalid.pos[1] = std::numeric_limits<float>::infinity();
  CHECK(!mesher.ShellSurfaceErrorM(std::span(&invalid, 1)),
        "nonfinite coordinates cannot publish a shell bound");
  for (const bool distantAnchor : {false, true}) {
    plan.AnchorEcef = distantAnchor ? Vec3{} : localAnchor;
    for (const double pitch : {0.0, 1.0}) {
      plan.PitchedShare = pitch;
      plan.Coarseness = LevelOfDetail::Massed;
      const auto plannedError = mesher.ShellSurfaceErrorM(plan, *scratch);
      CHECK(plannedError && *plannedError > 0.16,
            "an unmeshed plan bounds the future shell independently of its selected product");
      Raised fine;
      Raised shell;
      plan.Coarseness = LevelOfDetail::Fine;
      CHECK(mesher.Mesh(plan, *scratch, fine), "complete fine geometry exists");
      plan.Coarseness = LevelOfDetail::Shell;
      CHECK(mesher.Mesh(plan, *scratch, shell), "complete shell geometry exists");
      const auto error = mesher.ShellSurfaceErrorM(fine.WallCorners);
      CHECK(error && plannedError && *plannedError >= *error,
            "the plan bound covers the measured coordinate magnitude before allocating vertices");
      CHECK(error && *error > 0.16 && (distantAnchor || *error == 0.25),
            "the bound includes recess depth and coordinate-dependent rounding");
      StructureSurfaceRefinementTask oracle;
      CHECK(oracle.Reset(
                {fine, shell, 61},
                {.MaxTriangleQueries = 131072, .MaxRegions = 4096, .TargetUncertaintyM = 0.00025}),
            "the independent complete-surface oracle accepts both native streams");
      bool complete = false;
      for (size_t step = 0; step < 40000; ++step) {
        const auto progress = oracle.Step(61, {.MaxWorkUnits = 256});
        if (!progress) { break; }
        if (*progress == StructureSurfaceErrorProgress::Complete) {
          complete = true;
          break;
        }
      }
      const auto measured = oracle.Bound(61);
      std::printf("pitch %.0f anchor %d: shell %.6f, oracle %.6f..%.6f, %zu queries\n",
                  pitch,
                  distantAnchor,
                  error.value_or(-1),
                  measured ? measured->LowerDistanceM() : -1,
                  measured ? measured->UpperDistanceM() : -1,
                  oracle.TriangleQueries());
      CHECK(complete && measured && error && measured->UpperDistanceM() <= *error,
            "all triangles in both directions fit the analytical shell bound");
      CHECK(measured && plannedError && measured->UpperDistanceM() <= *plannedError,
            "the independent surface oracle also fits the pre-mesh plan bound");
    }
  }
  return Report();
}
