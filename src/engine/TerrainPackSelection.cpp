#include "EngineState.h"
#include "Box.h"
#include "GroundMesher.h"
#include "GroundPatchwork.h"
#include "ProjectedErrorBudget.h"
#include "SourceProductSelection.h"
#include "TerrainRefinement.h"
#include "TangentFrame.h"
#include "TileGeodesy.h"
#include "Units.h"
#include "Vec2.h"
#include "Vec3.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>

namespace outshine {
namespace {
constexpr double kSurfaceDerivativeMostM = 6400000;

Box TerrainBoundsInFrame(const Box &bounds, const TangentFrame &frame) {
  Box local;
  for (unsigned corner = 0; corner < 8; ++corner) {
    const auto point = frame.ToLocalPosition(bounds.Corner(corner));
    local.Cover(Vec3{{point.EastM, point.UpM, -point.NorthM}});
  }
  return local;
}

bool IntersectsNearTerrain(const Box &local, const Box &eyes, double reachM) {
  return local.Min[0] <= eyes.Max[0] + reachM && local.Max[0] >= eyes.Min[0] - reachM &&
         local.Min[2] <= eyes.Max[2] + reachM && local.Max[2] >= eyes.Min[2] - reachM;
}

double HorizontalGap(const Box &local) {
  return std::hypot(std::max({0.0, local.Min[0], -local.Max[0]}),
                    std::max({0.0, local.Min[2], -local.Max[2]}));
}

struct FocusWindow {
  TangentFrame Frame;
  double NormalVariation;
  double SurfaceOffsetM;

  [[nodiscard]] double Separation(const Box &bounds) const {
    const Box local = TerrainBoundsInFrame(bounds, Frame);
    const auto extent = [&local](size_t axis) {
      return std::max(std::abs(local.Min[axis]), std::abs(local.Max[axis]));
    };
    const double range = std::hypot(extent(0), extent(1), extent(2));
    const double guard =
        128 * std::numeric_limits<double>::epsilon() * (kSurfaceDerivativeMostM + range);
    return std::max(0.0, HorizontalGap(local) - SurfaceOffsetM - NormalVariation * range - guard);
  }
};

FocusWindow CameraWindow(const Ground::GeoBounds &bounds) {
  const double angle =
      0.5 * kDeg2Rad * (bounds.MaxLonDeg - bounds.MinLonDeg + bounds.MaxLatDeg - bounds.MinLatDeg);
  return {.Frame =
              TangentFrame::At({.LongitudeDeg = std::midpoint(bounds.MinLonDeg, bounds.MaxLonDeg),
                                .LatitudeDeg = std::midpoint(bounds.MinLatDeg, bounds.MaxLatDeg)}),
          .NormalVariation = std::min(angle, 1.0),
          .SurfaceOffsetM = angle * kSurfaceDerivativeMostM};
}

Generators::Osm::GroundTerrainErrorLimit SweepErrorLimit(const TerrainSweep &sweep,
                                                         const TangentFrame &frame,
                                                         const Vec2 &viewport,
                                                         ProjectedErrorBudget projection,
                                                         double reachM) {
  if (!sweep.EyePath) { return {}; }
  const auto path = ViewPathInEcef(*sweep.EyePath, frame);
  const auto eyes = sweep.Eyes;
  return [frame, path, viewport, projection, eyes, reachM](const Box &bounds) {
    if (IntersectsNearTerrain(TerrainBoundsInFrame(bounds, frame), eyes, reachM)) { return -1.0; }
    return SourceProductSelection::GeometryErrorLimitM(bounds, path, viewport, projection);
  };
}
}

Generators::Osm::GroundTerrainErrorLimit
Engine::State::TerrainPackErrorLimit(const Around &coverage) const {
  if (!Picture.Standing) { return {}; }
  const auto &eye =
      Picture.Standing->Watched() ? Picture.Standing->Watching() : Picture.Standing->Aimed();
  if (eye.Kind != Render::CameraKind::Perspective) { return {}; }
  const Vec2 viewport{
      {static_cast<double>(Picture.Frame.WidthPx), static_cast<double>(Picture.Frame.HeightPx)}};
  const ProjectedErrorBudget projection{.FocalPx = viewport[1] / (2 * std::tan(eye.YfovRad * 0.5)),
                                        .AllowedErrorPx =
                                            Generators::TerrainRefinementDetail{}.ErrorPx};
  const double reachM = FinestTerrainSampleRadiusM();
  if (coverage.Sweep) {
    const auto anchor = Session.Declared.Ground.Origin;
    const TangentFrame frame =
        TangentFrame::At({.LongitudeDeg = anchor.LongitudeDeg, .LatitudeDeg = anchor.LatitudeDeg});
    return SweepErrorLimit(*coverage.Sweep, frame, viewport, projection, reachM);
  }
  const auto bounds = PatchworkFocusWindow(coverage);
  if (!bounds) { return {}; }
  return [window = CameraWindow(*bounds), viewport, projection, reachM](const Box &pack) {
    const double distance = window.Separation(pack);
    if (distance <= reachM) { return -1.0; }
    return SourceProductSelection::GeometryErrorLimitM(distance, viewport, projection);
  };
}
}
