#include "TriangleDistance.h"
#include "Check.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const Vec3 a{{0, 0, 0}}, b{{2, 0, 0}}, c{{0, 2, 0}};

  struct Case {
    Vec3 Point;
    double Distance;
    Vec3 Target;
  };

  const std::array cases{Case{Vec3{{0.5, 0.5, 3}}, 3, Vec3{{0.5, 0.5, 0}}},
                         Case{Vec3{{1, -2, 0}}, 2, Vec3{{1, 0, 0}}},
                         Case{Vec3{{-3, -4, 0}}, 5, a},
                         Case{Vec3{{2, 2, 0}}, std::sqrt(2.0), Vec3{{1, 1, 0}}},
                         Case{Vec3{{0.25, 0.25, 0}}, 0, Vec3{{0.25, 0.25, 0}}}};
  for (const auto &test : cases) {
    const auto bound = BoundPointTriangleDistance(test.Point, a, b, c);
    CHECK(bound && bound->UpperDistanceM >= test.Distance &&
              bound->UpperDistanceM <= test.Distance + 1e-12,
          "face, edge, vertex and on-surface analytic distances are enclosed tightly");
    CHECK(bound && bound->LowerDistanceM <= test.Distance &&
              bound->LowerDistanceM >= std::max(0.0, test.Distance - 1e-12),
          "support-plane lower bound tightly encloses the independently known distance");
    if (bound) {
      for (size_t axis = 0; axis < 3; ++axis) {
        CHECK(std::abs(bound->TargetPointEstimateM[axis] - test.Target[axis]) < 1e-12,
              "returned target position matches the independently known primitive point");
      }
    }
  }
  const auto collapsed = BoundPointTriangleDistance(Vec3{{3, 4, 0}}, a, a, a);
  CHECK(collapsed && collapsed->UpperDistanceM >= 5 && collapsed->UpperDistanceM < 5 + 1e-12,
        "collapsed triangle remains a valid point target");
  const auto collinear = BoundPointTriangleDistance(Vec3{{1, 3, 0}}, a, b, Vec3{{4, 0, 0}});
  CHECK(collinear && collinear->UpperDistanceM >= 3 && collinear->UpperDistanceM < 3 + 1e-12,
        "collinear triangle remains a valid segment target");
  const auto transform = [](const Vec3 &p) {
    return Vec3{{1000 - 7 * p[1], -2000 + 7 * p[0], 3000 + 7 * p[2]}};
  };
  for (const auto &test : cases) {
    const auto bound =
        BoundPointTriangleDistance(transform(test.Point), transform(a), transform(b), transform(c));
    CHECK(bound && bound->UpperDistanceM >= 7 * test.Distance &&
              bound->UpperDistanceM < 7 * test.Distance + 1e-9,
          "translation, rotation and metric scale preserve conservative distance bounds");
  }

  // Independent Decimal (200 digits) upward-rounded norms expose final-nextafter-only errors.
  struct RoundedCase {
    Vec3 Point;
    double Ceiling;
  };

  const std::array roundedCases{
      RoundedCase{Vec3{{0x1.296d2a48e13e0p+1, 0x1.6e2687c391368p+2, 0x1.3160e7c6ecef0p-1}},
                  0x1.8d09a72cd0711p+2},
      RoundedCase{Vec3{{0x1.d8cff53d761e0p-1, -0x1.120fd26e22722p+3, 0x1.ec798ab33532cp+2}},
                  0x1.719db0bd89341p+3},
      RoundedCase{Vec3{{0x1.39dabeb62a53ap+3, -0x1.1745c97ce756fp+3, -0x1.3df5beb38c660p-1}},
                  0x1.a4960fdb179dcp+3},
      RoundedCase{Vec3{{-0x1.ea0bdfcf71900p-4, -0x1.87c49257a225cp+1, -0x1.1c59f182832a4p+1}},
                  0x1.e4539c28c75d3p+1},
      RoundedCase{Vec3{{-0x1.acedfc2708000p-5, -0x1.8b5a752de74dfp+2, 0x1.5007e1dd75748p+1}},
                  0x1.ad962d7ccdf57p+2},
      RoundedCase{Vec3{{-0x1.05e77c0bf1e76p+3, -0x1.1a03582c62552p+3, 0x1.f7dac9527e348p+0}},
                  0x1.85fd581f7432cp+3}};
  for (const auto &test : roundedCases) {
    const auto bound = BoundPointTriangleDistance(test.Point, a, a, a);
    CHECK(bound && bound->UpperDistanceM >= test.Ceiling,
          "outward intermediate arithmetic encloses high-precision norm controls");
    const double floor = std::nextafter(test.Ceiling, -std::numeric_limits<double>::infinity());
    CHECK(bound && bound->LowerDistanceM <= floor && bound->LowerDistanceM >= floor - 1e-12,
          "lower endpoint remains below independently downward-rounded Decimal norm");
  }
  const double infinity = std::numeric_limits<double>::infinity();
  const double nan = std::numeric_limits<double>::quiet_NaN();
  CHECK(!BoundPointTriangleDistance(Vec3{{infinity, 0, 0}}, a, b, c),
        "nonfinite source point is refused");
  CHECK(!BoundPointTriangleDistance(a, Vec3{{nan, 0, 0}}, b, c),
        "nonfinite target geometry is refused");
  const double extreme = std::numeric_limits<double>::max();
  CHECK(!BoundPointTriangleDistance(Vec3{{extreme, extreme, extreme}}, a, b, c),
        "unrepresentable distance produces unknown instead of optimistic finite error");
  return Report();
}
