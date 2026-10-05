#include "LightVisibilityStage.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace {
outshine::Vec3 Project(const outshine::Mat4 &matrix, const outshine::Vec3 &point) {
  outshine::Vec3 result;
  for (size_t row = 0; row < 3; ++row) {
    result[row] = matrix[12 + row];
    for (size_t axis = 0; axis < 3; ++axis) { result[row] += matrix[axis * 4 + row] * point[axis]; }
  }
  return result;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Render;
  using namespace outshine::Test;
  LightVisibilityStage stage;
  const LightVisibilityStage::Overhead sun{.ToSun = {{0, 0, 1}}, .Up = {{0, 1, 0}}};
  stage.Declare(sun, 10000, true);
  CHECK(stage.Standing() && stage.RegionCount() == 4, "world shadows have four regions");
  const Vec3 eye{{20000.125, -30000.125, 100.0}};
  const Vec3 relative{{-eye[0], -eye[1], -eye[2]}};
  stage.Build(relative);
  CHECK(stage.StoodAtM() == eye, "world shadows are centred at the eye independently of rotation");
  const std::array<Mat4, 4> original{stage.RegionProjection(0),
                                     stage.RegionProjection(1),
                                     stage.RegionProjection(2),
                                     stage.RegionProjection(3)};
  for (size_t region = 0; region < 4; ++region) {
    const auto transform = stage.Regions()[region].Transform;
    for (const Vec3 point :
         std::array<Vec3, 3>{{{{0, 0, 0}}, {{120, -50, 800}}, {{-90, 70, 2500}}}}) {
      const Vec3 base = Project(stage.LightFromWorld(), point);
      const Vec3 actual = Project(stage.RegionProjection(region), point);
      CHECK_NEAR(base[0] * transform[0] + transform[2],
                 actual[0],
                 2e-5,
                 "region X",
                 "GPU region transform matches the independently projected point");
      CHECK_NEAR(base[1] * transform[1] + transform[3],
                 actual[1],
                 2e-5,
                 "region Y",
                 "GPU region transform matches the independently projected point");
      CHECK_NEAR(base[2], actual[2], 1e-12, "depth", "all regions retain full-world caster depth");
    }
    CHECK(stage.Regions()[region].Atlas[3] > 0 && stage.Regions()[region].Atlas[3] < 0.001,
          "bias is finite and follows world texel size rather than a fixed depth fraction");
  }
  CHECK_NEAR(std::abs(original[0][0]),
             1.0 / 256.0,
             1e-12,
             "near scale",
             "near coverage remains 512 metres across a large world");
  for (const Vec3 caster :
       std::array<Vec3, 2>{{{{-10000, -10000, -10000}}, {{10000, 10000, 10000}}}}) {
    const Vec3 projected = Project(stage.LightFromWorld(), caster - eye);
    CHECK(std::abs(projected[0]) <= 1 && std::abs(projected[1]) <= 1 && projected[2] >= 0 &&
              projected[2] <= 1,
          "far coverage includes world casters behind and away from the camera");
  }
  stage.Build(relative);
  for (size_t region = 0; region < 4; ++region) {
    CHECK(stage.RegionProjection(region) == original[region],
          "repeated framing without movement preserves every region");
  }
  stage.Declare({.ToSun = {{0, 1, 0}}, .Up = {{0, 1, 0}}}, 10000, true);
  CHECK(stage.Standing(), "zenith light has a defined alternate basis");
  stage.Build(relative);
  CHECK(std::ranges::all_of(stage.LightFromWorld(),
                            [](double value) { return std::isfinite(value); }),
        "parallel light and up produce a finite projection");
  stage.Declare(sun, std::numeric_limits<double>::infinity(), true);
  CHECK(!stage.Standing(), "an infinite radius is refused at declaration");
  return Report();
}
