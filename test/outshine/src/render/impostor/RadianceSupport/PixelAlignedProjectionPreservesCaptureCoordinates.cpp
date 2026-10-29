#include "RadianceSupport.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;

void CheckProjection(Viewpoint camera, RadiancePlane source) {
  const auto lens = Lens::From(camera, 1280, 720);
  CHECK(lens, "the independent projection uses the actual GPU lens");
  if (!lens) { return; }
  auto jittered = *lens;
  jittered.Jitter = {{.375f, -.125f}};
  const auto geometry = BuildProjectedRadianceSupport(source, camera, jittered);
  CHECK(geometry && geometry->parts() == 1, "valid views produce one four-vertex support");
  if (!geometry) { return; }
  const auto points = geometry->positionsOf(0);
  const auto uv = geometry->textureOf(0);
  CHECK(points.size() == 12 && uv.size() == 8, "support has constant geometric cost");
  const auto matrix = jittered.ViewProjection(camera);
  for (size_t vertex = 0; vertex < 4; ++vertex) {
    std::array<double, 4> clip{};
    for (size_t row = 0; row < 4; ++row) {
      clip[row] = matrix[12 + row];
      for (size_t axis = 0; axis < 3; ++axis) {
        clip[row] += matrix[4 * axis + row] *
                     (static_cast<double>(points[3 * vertex + axis]) - camera.EyeM[axis]);
      }
    }
    const std::array pixel{(clip[0] / clip[3] + 1) * 640, (1 - clip[1] / clip[3]) * 360};
    for (const double coordinate : pixel) {
      CHECK(std::isfinite(coordinate) && std::abs(coordinate - std::round(coordinate)) < .001,
            "independent GPU-matrix projection lands on integer pixel boundaries including jitter");
    }
    CHECK(std::abs(uv[2 * vertex] - (.5 + (points[3 * vertex] - source.Centre[0]) /
                                              (2 * source.HalfExtentM))) < .00001 &&
              std::abs(uv[2 * vertex + 1] - (.5 - (points[3 * vertex + 1] - source.Centre[1]) /
                                                      (2 * source.HalfExtentM))) < .00001,
          "expanded support retains the original capture coordinates instead of stretching its "
          "image");
  }
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  Viewpoint camera;
  camera.Kind = CameraKind::Orthographic;
  camera.EyeM = {{.02, .03, 4}};
  camera.XMagM = camera.YMagM = 4;
  camera.ZNearM = .1;
  camera.ZFarM = 10;
  const RadiancePlane source{.Centre = {}, .TowardEye = {{0, 0, 1}}, .HalfExtentM = 1};
  CheckProjection(camera, source);
  auto perspective = camera;
  perspective.Kind = CameraKind::Perspective;
  perspective.EyeM = {{13.02, 7.03, 64000}};
  perspective.YfovRad = .8;
  perspective.ZFarM = 100000;
  auto far = source;
  far.HalfExtentM = 300;
  CheckProjection(perspective, far);
  const auto lens = Lens::From(camera, 1280, 720);
  if (!lens) { return Report(); }
  auto invalid = source;
  invalid.HalfExtentM = std::numeric_limits<double>::infinity();
  CHECK(!BuildProjectedRadianceSupport(invalid, camera, *lens),
        "unbounded support is refused before allocation");
  invalid = source;
  invalid.TowardEye = {{0, 0, -1}};
  CHECK(!BuildProjectedRadianceSupport(invalid, camera, *lens),
        "backside views grant no replacement");
  invalid = source;
  invalid.Centre = {{1000000, 0, 0}};
  CHECK(!BuildProjectedRadianceSupport(invalid, camera, *lens),
        "offscreen products have no fine support demand");
  invalid = source;
  invalid.TowardEye = {{1, 0, 1}};
  camera.EyeM = {{0, 0, .2}};
  CHECK(!BuildProjectedRadianceSupport(invalid, camera, *lens),
        "near-plane crossings preserve the original");
  return Report();
}
