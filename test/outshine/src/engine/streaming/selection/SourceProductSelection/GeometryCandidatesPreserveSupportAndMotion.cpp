#include "SourceProductSelection.h"
#include "Viewing.h"
#include "Vec2.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
using Selection = SourceProductSelection;

std::shared_ptr<const PlanHierarchy> Hierarchy() {
  const std::array boxes{Box{.Min = {{-10, -2, 0}}, .Max = {{-8, 2, 4}}},
                         Box{.Min = {{8, -2, 0}}, .Max = {{10, 2, 4}}}};
  auto hierarchy = PlanHierarchy::Build(boxes);
  CHECK(hierarchy, "two bounded source geometries have a complete hierarchy");
  return hierarchy ? std::make_shared<const PlanHierarchy>(std::move(*hierarchy)) : nullptr;
}

void SourceSupport() {
  const auto hierarchy = Hierarchy();
  if (!hierarchy) { return; }
  std::array<Selection::Source, 2> sources;
  sources.fill({.Cohort = 1000, .FieldSupported = false, .Faces = 8, .ShellErrorM = 0.25});
  sources[1].ShellErrorM = 0.5;
  const auto selector = Selection::Of(hierarchy, sources, "revision");
  CHECK(selector, "geometry support does not require a material-field capture");
  if (!selector) { return; }
  Render::CameraBasis camera{.EyeM = {{0, 0, 1000}}};
  const ProjectedErrorBudget projection{.FocalPx = 1000, .AllowedErrorPx = 1};
  const auto shell = selector->SelectGeometry(camera, projection);
  CHECK(shell && shell->Products.size() == 1 &&
            shell->Products[0].Representation == Selection::Kind::Shell &&
            shell->Products[0].WorldErrorM == 0.5,
        "a complete shell group uses the maximum child error rather than an average");
  camera.EyeM[2] = 1000000;
  const auto hull = selector->SelectGeometry(camera, projection);
  CHECK(hull && hull->Products.size() == 1 &&
            hull->Products[0].Representation == Selection::Kind::Hull,
        "a sufficiently small group selects one bounded hull candidate");
  sources[1].Faces = 0;
  sources[1].ShellErrorM.reset();
  const auto unknown = Selection::Of(hierarchy, sources, "revision");
  const auto split = unknown->SelectGeometry(camera, projection);
  size_t covered = 0;
  size_t unresolved = 0;
  for (const auto &product : split->Products) {
    const auto members = unknown->Members(*split, product);
    CHECK(members, "each product belongs to the immutable source revision");
    if (members) { covered += members->size(); }
    unresolved += product.Representation == Selection::Kind::Unresolved ? 1 : 0;
  }
  CHECK(covered == 2 && unresolved == 1 && split->Products.size() == 2,
        "an unknown source is retained and prevents implicit group replacement");
  sources[1] = {.Cohort = 2000, .FieldSupported = false, .Faces = 8, .ShellErrorM = std::nullopt};
  const auto cohorts = Selection::Of(hierarchy, sources, "revision");
  const auto partition = cohorts->SelectGeometry(camera, projection, 0, false, true);
  CHECK(partition && partition->Products.size() == 2,
        "cohort-constrained geometry products never combine independent source owners");
  camera.EyeM[2] = 5;
  const auto original = cohorts->SelectGeometry(camera, projection);
  CHECK(original && original->Products.size() == 2 &&
            original->Products[1].Representation == Selection::Kind::Geometry,
        "known geometry with unknown shell error remains original near the projection pole");
  const auto fields = cohorts->Select(camera.EyeM, projection);
  CHECK(fields && fields->Products[0].Representation == Selection::Kind::Geometry,
        "field-unsupported geometry is not mislabeled as an unresolved source");
}

Vec2 Project(const Vec3 &point, const Render::CameraBasis &camera, double focal) {
  const auto relative = point - camera.EyeM;
  return {{focal * Dot(relative, camera.Right) / Dot(relative, camera.Forward),
           focal * Dot(relative, camera.Up) / Dot(relative, camera.Forward)}};
}

double ObservedShellError(const Box &bounds,
                          const Render::CameraBasis &camera,
                          ProjectedErrorBudget projection) {
  double observed = 0;
  for (unsigned corner = 0; corner < 8; ++corner) {
    for (int x = -1; x <= 1; ++x) {
      for (int y = -1; y <= 1; ++y) {
        for (int z = -1; z <= 1; ++z) {
          Vec3 direction{{static_cast<double>(x), static_cast<double>(y), static_cast<double>(z)}};
          if (!Normalise(direction)) { continue; }
          const auto point = bounds.Corner(corner);
          for (size_t eyeAxis = 0; eyeAxis < 3; ++eyeAxis) {
            auto displaced = camera;
            displaced.EyeM[eyeAxis] += 100;
            const auto a = Project(point, displaced, projection.FocalPx);
            const auto b = Project(point + direction * 0.5, displaced, projection.FocalPx);
            observed = std::max(observed, std::abs(a[0] - b[0]) + std::abs(a[1] - b[1]));
          }
        }
      }
    }
  }
  return observed;
}

void MotionBounds() {
  const auto hierarchy = Hierarchy();
  if (!hierarchy) { return; }
  std::array<Selection::Source, 2> sources;
  sources.fill({.Cohort = 1000, .FieldSupported = false, .Faces = 8, .ShellErrorM = 0.5});
  const auto selector = Selection::Of(hierarchy, sources, "revision");
  if (!selector) { return; }
  const Render::CameraBasis camera{.EyeM = {{0, 0, 1000}}};
  const ProjectedErrorBudget projection{.FocalPx = 1000, .AllowedErrorPx = 1};
  const auto stationary = selector->SelectGeometry(camera, projection);
  const auto moving = selector->SelectGeometry(camera, projection, 100);
  CHECK(stationary && moving && moving->Products.size() == 1 &&
            moving->Products[0].Node == stationary->Products[0].Node &&
            moving->Products[0].Representation == stationary->Products[0].Representation &&
            moving->Products[0].ProjectedErrorPx >= stationary->Products[0].ProjectedErrorPx,
        "a movement domain retains logical product identity while enlarging projected error");
  const auto &bounds = hierarchy->Nodes()[0].Bounds;
  const double observed = ObservedShellError(bounds, camera, projection);
  CHECK(moving && observed <= moving->Products[0].ProjectedErrorPx,
        "declared shell bound dominates sampled 3D displacements throughout the eye domain");
  const auto ortho = selector->SelectGeometry(camera, {.FocalPx = 1, .AllowedErrorPx = 1}, 0, true);
  const auto movedOrtho =
      selector->SelectGeometry(camera, {.FocalPx = 1, .AllowedErrorPx = 1}, 100000, true);
  CHECK(ortho && movedOrtho && ortho->Products[0].Node == movedOrtho->Products[0].Node &&
            ortho->Products[0].ProjectedErrorPx == movedOrtho->Products[0].ProjectedErrorPx,
        "shared orthographic eye translation changes no geometric separation");
  auto invalid = camera;
  invalid.Right = invalid.Right * 2.0;
  CHECK(!selector->SelectGeometry(invalid, projection) &&
            !selector->SelectGeometry(camera, projection, -1) &&
            !selector->SelectGeometry(camera, {.FocalPx = 0, .AllowedErrorPx = 1}),
        "nonorthonormal cameras and invalid projection domains are rejected");
  sources[0].ShellErrorM = std::numeric_limits<double>::infinity();
  CHECK(!Selection::Of(hierarchy, sources, "revision"), "nonfinite source error is rejected");
}
}

int main() {
  SourceSupport();
  MotionBounds();
  return outshine::Test::Report();
}
