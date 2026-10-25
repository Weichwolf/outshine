#include "ViewReuseDomain.h"
#include "Check.h"
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr ProjectedErrorBudget kProjection{.FocalPx = 720};
  constexpr double kRadiusM = 4;
  ViewReuseDomain domain{
      .Eye = {}, .EyeEcef = Vec3{}, .Projection = kProjection, .EyeRadiusM = kRadiusM};
  CHECK(domain.Contains({}, kProjection, Vec3{{kRadiusM, 0, 0}}),
        "the exact translation boundary is reusable");
  CHECK(!domain.Contains({}, kProjection, Vec3{{kRadiusM + 1, 0, 0}}),
        "movement outside the qualified radius requires new selection");
  CHECK(!domain.Contains({}, {.FocalPx = kProjection.FocalPx / 2}, Vec3{}),
        "another projection cannot reuse the qualified selection");
  CHECK(!domain.Contains({}, kProjection), "geographic and ECEF distance domains cannot mix");
  domain.EyeEcef.reset();
  CHECK(domain.Contains({}, kProjection), "unchanged geographic eyes are inside their domain");
  constexpr double kOutsideLongitudeDeg = 0.001;
  CHECK(!domain.Contains({.LongitudeDeg = kOutsideLongitudeDeg}, kProjection),
        "geodesic separation beyond the radius requires new selection");
  domain.EyeRadiusM = std::numeric_limits<double>::quiet_NaN();
  CHECK(!domain.Contains({}, kProjection), "unknown domain bounds never grant reuse");
  return Report();
}
