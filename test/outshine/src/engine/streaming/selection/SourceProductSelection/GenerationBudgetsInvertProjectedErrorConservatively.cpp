#include "SourceProductSelection.h"
#include "Viewing.h"
#include "Check.h"
#include <limits>
#include <numbers>
#include <cmath>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  using Selection = SourceProductSelection;
  const Box bounds{.Min = {{-10, -10, -10}}, .Max = {{10, 10, 10}}};
  const Render::CameraBasis camera{.EyeM = {{0, 0, 1000}}};
  for (const double distance : {20.0, 100.0, 1000.0, 100000.0}) {
    auto moved = camera;
    moved.EyeM[2] = distance;
    for (const double focal : {100.0, 500.0, 2000.0}) {
      for (const double pixels : {0.01, 0.5, 1.0, 4.0}) {
        const ProjectedErrorBudget budget{.FocalPx = focal, .AllowedErrorPx = pixels};
        const double error = Selection::GeometryErrorLimitM(bounds, moved, budget);
        CHECK(error > 0 && Selection::GeometryErrorPx(bounds, moved, focal, error) <= pixels,
              "every returned positive generation budget passes the existing forward bound");
        CHECK(Selection::GeometryErrorPx(bounds, moved, focal, error * 1.001) > pixels,
              "the inverse stays close to the actual admitted boundary");
        CHECK(Selection::GeometryErrorLimitM(bounds, moved, budget, 1) < error,
              "eye movement reserve tightens the generation budget");
      }
    }
  }
  const ProjectedErrorBudget budget{.FocalPx = 500, .AllowedErrorPx = 1};
  const double ordinary = Selection::GeometryErrorLimitM(bounds, camera, budget);
  auto side = camera;
  side.EyeM[0] = 2000;
  CHECK(Selection::GeometryErrorLimitM(bounds, side, budget) < ordinary,
        "off-axis projection allows less error even at unchanged forward depth");
  auto near = camera;
  near.EyeM = {};
  CHECK(Selection::GeometryErrorLimitM(bounds, near, budget) == 0,
        "a box intersecting the eye plane grants no positive-error generation");
  auto invalid = camera;
  invalid.Forward = {};
  CHECK(Selection::GeometryErrorLimitM(bounds, invalid, budget) == 0 &&
            Selection::GeometryErrorLimitM(bounds, camera, {.FocalPx = 0, .AllowedErrorPx = 1}) ==
                0 &&
            Selection::GeometryErrorLimitM(bounds, camera, {.FocalPx = 500, .AllowedErrorPx = 0}) ==
                0 &&
            Selection::GeometryErrorLimitM(bounds, camera, budget, -1) == 0,
        "invalid camera, projection, motion or zero-error demand cannot admit approximation");
  const double orthographic = Selection::GeometryErrorLimitM(bounds, camera, budget, 0, true);
  CHECK(orthographic > 0 && orthographic * 500 * std::numbers::sqrt2 <= 1,
        "orthographic generation uses pixels per metre without a distance multiplier");
  CHECK(Selection::GeometryErrorLimitM(bounds, near, budget, 0, true) == orthographic,
        "orthographic error admission is independent of eye depth");
  CHECK(Selection::GeometryErrorLimitM(
            bounds,
            camera,
            {.FocalPx = std::numeric_limits<double>::infinity(), .AllowedErrorPx = 1}) == 0,
        "nonfinite projection input cannot grant work");
  const Vec2 viewport{{1280, 720}};
  const double panorama = Selection::GeometryErrorLimitM(bounds, camera.EyeM, viewport, budget);
  const double scale = std::hypot(1.0, 1280.0 / 1000, 720.0 / 1000);
  const double displacement = 500 * std::hypot(std::numbers::sqrt2, 2.0);
  const double projected = displacement * panorama / ((990 - panorama) / scale - panorama);
  CHECK(panorama > 0 && projected <= 1,
        "the panoramic inverse includes both source expansion and all-direction depth scaling");
  CHECK(Selection::GeometryErrorLimitM(bounds, Vec3{{1000, 0, 0}}, viewport, budget) == panorama,
        "the panoramic demand does not depend on the direction from the source to the eye");
  CHECK(Selection::GeometryErrorLimitM(bounds, camera.EyeM, viewport, budget, 1) < panorama &&
            Selection::GeometryErrorLimitM(
                bounds, camera.EyeM, Vec2{{2560, 1440}}, {.FocalPx = 1000, .AllowedErrorPx = 1}) <
                panorama &&
            Selection::GeometryErrorLimitM(bounds, Vec3{}, viewport, budget) == 0,
        "motion and increased resolution tighten budgets; an intersecting eye preserves originals");
  auto badBounds = bounds;
  badBounds.Min[0] = std::numeric_limits<double>::quiet_NaN();
  CHECK(Selection::GeometryErrorLimitM(badBounds, camera, budget, 0, true) == 0 &&
            Selection::GeometryErrorLimitM(badBounds, camera.EyeM, viewport, budget) == 0,
        "nonfinite bounds cannot grant generation work in either projection model");
  return Report();
}
