#include "BuildingSourceView.h"
#include "BuildingMaterials.h"
#include "BuildingPlaneCapture.h"
#include "BuildingScratch.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numbers>
#include <stop_token>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

std::shared_ptr<PreparedStructureTile> Dome() {
  constexpr double metresPerDegree = 111320;
  const LongitudeLatitudeHeight at{.LongitudeDeg = 16, .LatitudeDeg = 48, .HeightM = 100};
  std::vector<double> ring;
  for (int corner = 0; corner < 8; ++corner) {
    const double angle = corner * std::numbers::pi / 4;
    ring.push_back(at.LatitudeDeg + 4 * std::sin(angle) / metresPerDegree);
    ring.push_back(at.LongitudeDeg +
                   4.8 * std::cos(angle) /
                       (metresPerDegree * std::cos(at.LatitudeDeg * std::numbers::pi / 180)));
  }
  StructurePlan plan;
  plan.RingLatLon = ring;
  plan.BaseAslM = plan.SeatAslM = plan.FootAslM = at.HeightM;
  plan.HeightM = 80;
  plan.HeightMeasured = true;
  plan.Coarseness = LevelOfDetail::Shell;
  plan.PitchedShare = 1;
  GeoToEcef(at, plan.AnchorEcef);
  BuildingScratch scratch;
  auto surface = BuildingSurface::Prepare(plan, scratch);
  CHECK(surface && surface->Shapes().size() == 1, "the native curved source prepares");
  if (!surface || surface->Shapes().size() != 1) { return {}; }
  CHECK(surface->Shapes()[0].Roof == RoofKind::Dome && surface->Shapes()[0].RiseM > 0,
        "the fixture has an actual raised dome");
  CHECK(!surface->SupportsProjection(), "analytical ray projection cannot represent this dome");
  Raised mesh;
  CHECK(surface->Mesh(scratch, mesh) && !mesh.WallRun.empty() && !mesh.RoofRun.empty(),
        "the same source already provides its complete native mesh");
  auto base = std::make_shared<PreparedStructureTile>();
  base->Structures.resize(1);
  base->Structures[0].Layout.Cell.Index = 1;
  base->Surfaces.push_back(std::move(*surface));
  return base;
}

void Originals(const std::shared_ptr<PreparedStructureTile> &base) {
  const auto &surface = base->Surfaces[0];
  const BuildingCaptureFrame frame{.Axes = surface.Axes(), .Origin = surface.Bounds().Middle()};
  const std::array<uint32_t, 1> cells{1};
  auto view = BuildingSourceView::Open(base, cells, frame);
  CHECK(view && view->Sources().size() == 1 && view->Unsupported().empty(),
        "whole original eligibility is independent of analytical ray support");
  if (!view) { return; }
  const BuildingSourceBudget budget{
      .Sources = 1, .Faces = surface.FaceCount(), .TriangleAttempts = 10000};
  const auto materials = BuildingSurfaceMaterials();
  const auto originals = view->Originals(budget, materials);
  BuildingScratch scratch;
  Raised expected;
  CHECK(surface.Mesh(scratch, expected).has_value(),
        "the independent curved original still meshes");
  size_t indices = 0;
  if (originals) {
    for (int part = 0; part < originals->parts(); ++part) {
      indices += originals->trianglesOf(part).size();
    }
  }
  CHECK(originals && indices == expected.WallRun.size() + expected.RoofRun.size(),
        "the complete curved original group retains every native triangle");
  auto limited = budget;
  limited.TriangleAttempts = 1;
  const auto rejected = view->Originals(limited, materials);
  CHECK(!rejected && rejected.error().Kind == BuildingSourceView::ErrorKind::WorkBudgetExceeded,
        "curved original emission still obeys its triangle budget");
  std::stop_source stop;
  stop.request_stop();
  const auto cancelled = view->Originals(budget, materials, stop.get_token());
  CHECK(!cancelled && cancelled.error().Kind == BuildingSourceView::ErrorKind::Cancelled,
        "curved original generation remains cancellable");
}

void AnalyticalRefusal(const std::shared_ptr<PreparedStructureTile> &base) {
  const auto &surface = base->Surfaces[0];
  const std::array sources{&surface};
  const auto capture = BuildingPlaneCapture::Prepare(sources);
  CHECK(capture.has_value(), "the optional analytical capture can index the source bounds");
  if (!capture) { return; }
  const auto result = capture->Capture({.Centre = surface.Bounds().Middle(),
                                        .TowardEye = surface.Axes().North,
                                        .HalfExtentM = 100,
                                        .Pixels = 3},
                                       {.Samples = 9, .SourceTests = 9});
  CHECK(!result && result.error().Kind == BuildingPlaneCapture::ErrorKind::UnsupportedSource &&
            result.error().Source == 0,
        "analytical capture still refuses an unsupported curved source explicitly");
}
}

int main() {
  if (const auto base = Dome()) {
    Originals(base);
    AnalyticalRefusal(base);
  }
  return outshine::Test::Report();
}
