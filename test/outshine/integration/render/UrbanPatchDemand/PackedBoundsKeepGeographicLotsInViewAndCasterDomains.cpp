#include "GeographicControls.h"
#include "InstanceDemand.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <numeric>
#include <vector>

namespace {
using namespace UrbanDemandControl;
using namespace outshine::Render;
using namespace outshine::Test;
constexpr double kEyeAboveGroundM = 100;

struct PatchControl {
  BatchSphere Bounds;
  std::vector<Vec3> Points;
};

std::vector<PatchControl>
Patches(const Osm::UrbanArea &area, std::array<double, 2> ground, const LocalFrame &frame) {
  std::vector<PatchControl> patches;
  for (uint32_t row = 0; row < area.Rows; row += kPatch) {
    for (uint32_t column = 0; column < area.Columns; column += kPatch) {
      const Osm::UrbanPatchRange range{.Column = column,
                                       .Row = row,
                                       .Columns = std::min(kPatch, area.Columns - column),
                                       .Rows = std::min(kPatch, area.Rows - row)};
      const auto ecef = area.BoundPatch(range, ground);
      CHECK(ecef, "each full or ragged patch obtains a geographic bound");
      if (!ecef) { continue; }
      const auto centre = frame.Point(ecef->CentreEcef);
      const auto packed = PackSphere({centre[0], centre[1], centre[2]}, ecef->RadiusM);
      CHECK(packed, "ECEF bounds pack outward in the common EUS frame");
      if (!packed) { continue; }
      patches.push_back({.Bounds = *packed, .Points = Corners(area, range, ground, frame)});
    }
  }
  return patches;
}

bool InSidePlanes(const Vec3 &point, const FrameContext &view) {
  const Vec3 shifted = point + view.PreViewTranslation;
  std::array<double, 4> clip{};
  for (size_t row = 0; row < clip.size(); ++row) {
    clip[row] = view.Mvp.At(row, 3);
    for (size_t axis = 0; axis < 3; ++axis) {
      clip[row] += static_cast<double>(view.Mvp.At(row, axis)) * shifted[axis];
    }
  }
  return clip[3] >= std::abs(clip[0]) && clip[3] >= std::abs(clip[1]);
}

void Enclosed(const PatchControl &patch) {
  const Vec3 centre{{patch.Bounds[0], patch.Bounds[1], patch.Bounds[2]}};
  for (const auto &point : patch.Points) {
    CHECK(Length(point - centre) <= patch.Bounds[3],
          "packed sphere encloses independently placed lot corners at both ASL extrema");
  }
}

void CheckDomains(const std::vector<PatchControl> &patches, double altitudeM, const Vec3 &offset) {
  std::vector<BatchSphere> bounds;
  bounds.reserve(patches.size());
  for (const auto &patch : patches) { bounds.push_back(patch.Bounds); }
  // A synthetic one-metre policy input, not a measured error of an urban parent.
  const std::vector<double> errors(bounds.size(), 1);
  std::vector<InstancePatchNeed> initial;
  size_t outside = 0;
  size_t casterOnly = 0;
  size_t visibleCorners = 0;
  size_t casterCorners = 0;
  for (size_t turn = 0; turn < 4; ++turn) {
    const double yaw = static_cast<double>(turn) * std::numbers::pi / 2;
    const auto view = ViewAt({.YawRad = yaw, .AltitudeM = altitudeM}, offset);
    const auto caster =
        ViewAt({.YawRad = yaw + std::numbers::pi / 2, .AltitudeM = altitudeM}, offset);
    const std::array views{BatchFrustum(view, {})};
    const std::array casters{BatchFrustum(caster, {})};
    const auto plan = PlanInstancePatches(bounds,
                                          errors,
                                          {.EyeM = offset + Vec3{{0, altitudeM, 0}},
                                           .Projection = {.FocalPx = 540},
                                           .Views = views,
                                           .Casters = casters});
    CHECK(plan && plan->size() == patches.size(), "every geographic patch keeps its identity");
    if (!plan) { return; }
    if (turn == 0) { initial = *plan; }
    for (size_t index = 0; index < patches.size(); ++index) {
      const auto &need = (*plan)[index];
      CHECK(need.Index == index && need.Detail == initial[index].Detail,
            "yaw preserves roundabout detail policy and original patch index");
      outside += static_cast<size_t>(!need.Visible);
      casterOnly += static_cast<size_t>(need.Caster && !need.Visible);
      for (const auto &point : patches[index].Points) {
        const bool visible = InSidePlanes(point, view);
        const bool shadowed = InSidePlanes(point, caster);
        visibleCorners += static_cast<size_t>(visible);
        casterCorners += static_cast<size_t>(shadowed);
        CHECK(!visible || need.Visible, "an in-view lot corner cannot be removed by patch culling");
        CHECK(!shadowed || need.Caster,
              "an offscreen shadow-domain corner cannot be removed by view culling");
      }
    }
  }
  CHECK(outside > 0 && casterOnly > 0, "geographic controls exercise real domain exclusions");
  CHECK(visibleCorners > 0 && casterCorners > 0, "both domains contain actual generated corners");
}

void GeographicCase(LongitudeLatitude focus, std::array<double, 2> ground, const Vec3 &offset) {
  const auto area = FieldAt(focus);
  const auto frame = FrameAt(focus, offset);
  const auto patches = Patches(area, ground, frame);
  CHECK(patches.size() == 25, "nonmultiple field sides retain all ragged edge patches");
  for (const auto &patch : patches) { Enclosed(patch); }
  CheckDomains(patches, std::midpoint(ground[0], ground[1]) + kEyeAboveGroundM, offset);
}

void UnknownTerrain() {
  const auto area = FieldAt({.LongitudeDeg = 0, .LatitudeDeg = 0});
  constexpr double unknown = std::numeric_limits<double>::quiet_NaN();
  const auto bound = area.BoundPatch({.Columns = kPatch, .Rows = kPatch}, {0, unknown});
  CHECK(!bound, "unknown terrain cannot produce a finite zero-height bound");
  const std::array<BatchSphere, 1> bounds{
      BatchSphere{0, 0, 0, std::numeric_limits<float>::quiet_NaN()}};
  const std::array errors{unknown};
  const std::array domain{
      BatchFrustum(ViewAt({.YawRad = 0, .AltitudeM = kEyeAboveGroundM}, {}), {})};
  const auto plan = PlanInstancePatches(
      bounds,
      errors,
      {.EyeM = {}, .Projection = {.FocalPx = 540}, .Views = domain, .Casters = domain});
  CHECK(plan && plan->front().Detail == InstanceDetail::Unknown && plan->front().Visible &&
            plan->front().Caster,
        "failed geographic bounds remain explicit unknown demand for both consumers");
}
}

int main() {
  const std::array<LongitudeLatitude, 5> sites{{{.LongitudeDeg = 0, .LatitudeDeg = 0},
                                                {.LongitudeDeg = 179.99, .LatitudeDeg = 48},
                                                {.LongitudeDeg = -179.99, .LatitudeDeg = -48},
                                                {.LongitudeDeg = 20, .LatitudeDeg = 84},
                                                {.LongitudeDeg = -20, .LatitudeDeg = -84}}};
  const std::array<std::array<double, 2>, 3> grounds{{{-400, -390}, {0, 30}, {4500, 5000}}};
  constexpr Vec3 farOrigin{{1000000.125, -2000000.25, 3000000.375}};
  for (const auto site : sites) {
    for (const auto ground : grounds) {
      GeographicCase(site, ground, {});
      GeographicCase(site, ground, farOrigin);
    }
  }
  UnknownTerrain();
  return Report();
}
