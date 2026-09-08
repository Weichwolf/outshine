#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <string>
#include <utility>
#include "Check.h"
#include "Lens.h"
#include "SceneRenderer.h"
#include "Shape.h"
#include "SubjectProxy.h"
#include "Viewing.h"

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;

static_assert(noexcept(Lens::From(std::declval<const Viewpoint &>(), 32, 32)));
static_assert(noexcept(std::declval<const Lens &>().Projection()));

void NumericalBoundary() {
  Viewpoint eye;
  eye.YfovRad = std::numbers::pi / 2;
  eye.ZNearM = 1;
  eye.ZFarM = 11;
  const auto finite = Lens::From(eye, 64, 32);
  CHECK(finite.has_value(), "a finite perspective lens is representable");
  if (finite) {
    const auto p = finite->Projection();
    CHECK_NEAR(p[0], 0.5f, 1e-6f, "NDC", "vertical FOV and aspect define x scale");
    CHECK_NEAR(p[5], 1.0f, 1e-6f, "NDC", "vertical FOV defines y scale");
    CHECK_NEAR(-p[10] + p[14], 1.0f, 1e-6f, "depth", "near maps to reverse-Z one");
    CHECK_NEAR((-11.0f * p[10] + p[14]) / 11.0f, 0.0f, 1e-6f, "depth", "far maps to zero");
  }
  for (double far : {0.0, std::numeric_limits<double>::infinity()}) {
    auto infinite = eye;
    infinite.ZFarM = far;
    const auto made = Lens::From(infinite, 64, 32);
    CHECK(made && made->Projection()[10] == 0 && made->Projection()[14] == 1,
          "both infinite perspective declarations retain reverse-Z depth");
  }
  const auto reject = [](const Viewpoint &bad) {
    CHECK(!Lens::From(bad, 64, 32), "invalid or nonrepresentable projection is rejected");
  };
  for (double invalid : {-1.0,
                         std::numeric_limits<double>::quiet_NaN(),
                         std::numeric_limits<double>::infinity(),
                         std::numeric_limits<double>::max(),
                         std::numeric_limits<double>::min()}) {
    auto bad = eye;
    bad.ZNearM = invalid;
    reject(bad);
    bad = eye;
    bad.YfovRad = invalid;
    reject(bad);
  }
  auto bad = eye;
  bad.ZFarM = std::nextafter(1.0, 2.0);
  reject(bad);
  bad = eye;
  bad.YfovRad = std::nextafter(std::numbers::pi, 0.0);
  reject(bad);
  bad = eye;
  bad.ZFarM = std::numeric_limits<double>::max();
  reject(bad);
  bad = eye;
  bad.ZNearM = 1e-38;
  bad.ZFarM = 1e38;
  reject(bad);
  bad = eye;
  bad.Kind = static_cast<CameraKind>(255);
  reject(bad);
  for (double extent : {0.0,
                        -1.0,
                        std::numeric_limits<double>::infinity(),
                        std::numeric_limits<double>::quiet_NaN(),
                        std::numeric_limits<double>::max()}) {
    CHECK(!Lens::From(eye, extent, 32) && !Lens::From(eye, 32, extent),
          "both viewport dimensions must be representable positive pixel counts");
  }
  eye.Kind = CameraKind::Orthographic;
  eye.XMagM = 2;
  eye.YMagM = 3;
  eye.ZNearM = 0;
  const auto ortho = Lens::From(eye, 64, 32);
  CHECK(ortho.has_value(), "orthographic near zero is valid");
  if (ortho) {
    const auto p = ortho->Projection();
    CHECK_NEAR(p[0], 0.5f, 1e-6f, "NDC", "xmag is a half extent");
    CHECK_NEAR(p[5], 1.0f / 3.0f, 1e-6f, "NDC", "ymag is independent of aspect");
    CHECK_NEAR(p[14], 1.0f, 1e-6f, "depth", "orthographic near zero maps to one");
    CHECK_NEAR(-11.0f * p[10] + p[14], 0.0f, 1e-6f, "depth", "orthographic far maps to zero");
  }
  for (double invalid : {0.0,
                         -1.0,
                         std::numeric_limits<double>::quiet_NaN(),
                         std::numeric_limits<double>::infinity(),
                         std::numeric_limits<double>::max(),
                         std::numeric_limits<double>::min()}) {
    bad = eye;
    bad.XMagM = invalid;
    reject(bad);
    bad = eye;
    bad.YMagM = invalid;
    reject(bad);
  }
  bad = eye;
  bad.ZNearM = -1;
  reject(bad);
  bad = eye;
  bad.ZNearM = std::numeric_limits<double>::min();
  reject(bad);
}

void FailedAimPreservesLens() {
  SceneRenderer renderer;
  const auto target = renderer.DrawsInto(32, 32, nullptr);
  CHECK(target.has_value(), "a real target supplies the camera viewport");
  if (!target) { return; }
  Eye view;
  view.Eye.YfovRad = 1;
  view.Eye.ZNearM = 0.05;
  view.Eye.ZFarM = 10;
  view.StandsInside = true;
  Shape shape;
  std::string error;
  CHECK(Aim(renderer, shape, view, {}, error), "bind an initial valid lens");
  const float original = renderer.NearMetres();
  const std::array<float, 3> positions = {0, 0, -0.1f};
  ShapePart part;
  part.VertexCount = 1;
  part.PositionsM = positions;
  shape.Parts = {&part, 1};
  view.Eye.ZNearM = 1;
  view.StandsInside = false;
  CHECK(!Aim(renderer, shape, view, {}, error) && !error.empty(),
        "geometry inside the new near plane rejects the candidate camera");
  CHECK(renderer.NearMetres() == original,
        "failed geometric validation preserves the previously published projection");
  view.StandsInside = true;
  CHECK(Aim(renderer, shape, view, {}, error) && renderer.NearMetres() == 1,
        "an explicitly permitted inside view publishes the new projection");
}
} // namespace

int main() {
  NumericalBoundary();
  const bool initialized = SDL_Init(SDL_INIT_VIDEO);
  CHECK(initialized, "SDL video initializes for the consumer boundary");
  if (initialized) {
    FailedAimPreservesLens();
    SDL_Quit();
  }
  return outshine::Test::Report();
}
