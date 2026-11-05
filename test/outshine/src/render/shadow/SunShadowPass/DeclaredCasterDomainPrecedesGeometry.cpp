#include "SunShadowPass.h"
#include "GeometryRenderer.h"
#include "RenderScene.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <limits>
#include <string>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;

std::array<Mat4, kSunShadowRegions> Projection(const SunShadowPass &pass) {
  std::array<Mat4, kSunShadowRegions> result{};
  for (size_t region = 0; region < pass.RegionCount(); ++region) {
    result[region] = pass.RegionProjection(region);
  }
  return result;
}

void Residency(bool cameraCentred) {
  constexpr double radius = 10000;
  const Vec3 centre{{300, -500, 200}};
  const Vec3 eye{{20000.125, -30000.125, 100}};
  const Vec3 preView = eye * -1.0;
  const SunShadowPass::Overhead sun{.ToSun = {{.3f, .8f, .5f}}, .Up = {{0, 1, 0}}};
  SunShadowPass pass;
  pass.Declare(sun, radius, cameraCentred, centre);
  CHECK(pass.IsDeclared(), "a complete caster domain is valid without geometry or a device");
  pass.Build(preView);
  const auto initial = Projection(pass);
  RenderScene scene;
  GeometryRenderer geometry(scene);
  pass.Binds(geometry);
  pass.Build(preView);
  CHECK(Projection(pass) == initial,
        "binding an empty renderer cannot change the declared shadow demand");
  Mat4 placement;
  placement.SetTranslation({{-40000, 60000, -8000}});
  std::string error;
  CHECK(geometry.SetPlacements(placement.data(), 1, error),
        "a newly resident source has an independent scene placement");
  pass.Build(preView);
  CHECK(Projection(pass) == initial,
        "resident caster positions cannot feed back into the declared projection");
  CHECK(geometry.SetPlacements(nullptr, 0, error), "source eviction clears its placement");
  pass.Build(preView);
  CHECK(Projection(pass) == initial, "source eviction cannot change the declared shadow demand");
  const Vec3 changed = centre + Vec3{{10, 20, 30}};
  pass.Declare(sun, radius, cameraCentred, changed);
  pass.Build(preView);
  CHECK(Projection(pass) != initial, "an explicit domain update changes the projection");
  pass.Declare(sun, radius, cameraCentred);
  pass.Build(preView);
  CHECK(Projection(pass) != initial,
        "a consumer can explicitly restore geometry-derived scene framing");
}

void Invalid() {
  SunShadowPass pass;
  const SunShadowPass::Overhead sun{.ToSun = {{0, 0, 1}}, .Up = {{0, 1, 0}}};
  for (const double invalid :
       {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
    pass.Declare(sun, 10000, true, Vec3{{invalid, 0, 0}});
    CHECK(!pass.IsDeclared() && pass.RegionCount() == 0,
          "nonfinite metadata cannot define a shadow domain");
  }
}
}

int main() {
  Residency(false);
  Residency(true);
  Invalid();
  return outshine::Test::Report();
}
