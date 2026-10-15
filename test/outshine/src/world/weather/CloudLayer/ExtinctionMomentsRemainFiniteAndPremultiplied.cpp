#include "CloudMoments.h"
#include "Check.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

namespace {

struct Case {
  double HeightM;
  double Cover;
  double ExtinctionPerM;
  outshine::Vec3 Direction;
  double MaximumM;
  double Weight;
  double WeightedDistanceM;
};

constexpr double kCloudBaseM = 2572.0;
constexpr double kBrokenCover = 0.65;
constexpr double kWeightAbsoluteTolerance = 1e-20;
constexpr double kWeightRelativeTolerance = 1e-11;
constexpr double kMomentAbsoluteToleranceM = 1e-18;
constexpr double kMomentRelativeTolerance = 1e-10;
constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr std::array kCases{Case{.HeightM = 1772.0,
                                 .Cover = kBrokenCover,
                                 .ExtinctionPerM = 0.012,
                                 .Direction = {{0.0, 1.0, 0.0}},
                                 .MaximumM = kInfinity,
                                 .Weight = 0.9934199987335914,
                                 .WeightedDistanceM = 946.6439745187288},
                            Case{.HeightM = 1772.0,
                                 .Cover = kBrokenCover,
                                 .ExtinctionPerM = 0.012,
                                 .Direction = {{1.0, 0.0, 0.0}},
                                 .MaximumM = kInfinity,
                                 .Weight = 1.0,
                                 .WeightedDistanceM = 102380.50022797214},
                            Case{.HeightM = 2800.0,
                                 .Cover = kBrokenCover,
                                 .ExtinctionPerM = 0.012,
                                 .Direction = {{1.0, 0.0, 0.0}},
                                 .MaximumM = kInfinity,
                                 .Weight = 1.0,
                                 .WeightedDistanceM = 95.51036112895788},
                            Case{.HeightM = 4000.0,
                                 .Cover = kBrokenCover,
                                 .ExtinctionPerM = 0.012,
                                 .Direction = {{0.0, -1.0, 0.0}},
                                 .MaximumM = kInfinity,
                                 .Weight = 0.9934199987335914,
                                 .WeightedDistanceM = 978.6826619588271},
                            Case{.HeightM = 1772.0,
                                 .Cover = 1.0,
                                 .ExtinctionPerM = 1e-14,
                                 .Direction = {{0.0, 1.0, 0.0}},
                                 .MaximumM = kInfinity,
                                 .Weight = 6.0117187499819295e-12,
                                 .WeightedDistanceM = 6.61289062497872e-09},
                            Case{.HeightM = 1772.0,
                                 .Cover = 1.0,
                                 .ExtinctionPerM = 100.0,
                                 .Direction = {{0.0, 1.0, 0.0}},
                                 .MaximumM = kInfinity,
                                 .Weight = 1.0,
                                 .WeightedDistanceM = 800.0550537634408},
                            Case{.HeightM = 1772.0,
                                 .Cover = kBrokenCover,
                                 .ExtinctionPerM = 0.012,
                                 .Direction = {{0.0, 1.0, 0.0}},
                                 .MaximumM = 1100.0,
                                 .Weight = 0.9213448301179492,
                                 .WeightedDistanceM = 863.5517271485407},
                            Case{.HeightM = 4000.0,
                                 .Cover = 1.0,
                                 .ExtinctionPerM = 1e-6,
                                 .Direction = {{0.9997485857950974, -0.022422426289382777, 0.0}},
                                 .MaximumM = kInfinity,
                                 .Weight = 0.08758656010510844,
                                 .WeightedDistanceM = 12575.15536558325},
                            Case{.HeightM = 4000.0,
                                 .Cover = 1.0,
                                 .ExtinctionPerM = 1e-6,
                                 .Direction = {{0.9997485857950974, -0.022422426289382777, 0.0}},
                                 .MaximumM = 95912.82826129103,
                                 .Weight = 0.04282635375536345,
                                 .WeightedDistanceM = 2853.241609101983}};

}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  for (const Case &sample : kCases) {
    const CloudLayer layer{.BaseM = kCloudBaseM,
                           .Cover = sample.Cover,
                           .ExtinctionPerM = sample.ExtinctionPerM,
                           .WindMs = CloudWindFrom({.FromDeg = 240.0, .SpeedMs = 8.0})};
    const Vec3 origin{{0.0, sample.HeightM, 0.0}};
    const CloudTrace trace{.Seconds = 37.0, .MaximumM = sample.MaximumM};
    const auto moments = TraceCloudMoments(layer, origin, sample.Direction, trace);
    // Independent NumPy cells plus analytic constant-cell Beer integration in cloud_moments.py.
    CHECK(std::abs(moments.Weight - sample.Weight) <
              std::max(kWeightAbsoluteTolerance, sample.Weight * kWeightRelativeTolerance),
          "native zeroth extinction moment matches the independent declared-field reference");
    CHECK(std::abs(moments.WeightedDistanceM - sample.WeightedDistanceM) <
              std::max(kMomentAbsoluteToleranceM,
                       sample.WeightedDistanceM * kMomentRelativeTolerance),
          "native first moment remains accurate for horizon, inside, thin and saturated rays");
    const double optical = CloudOpticalDepth(layer, origin, sample.Direction, trace);
    CHECK(moments.Weight == -std::expm1(-optical),
          "moments and shadow optical depth share exactly the same density cells");
    CHECK(std::isfinite(moments.WeightedDistanceM) && moments.WeightedDistanceM > 0.0,
          "a nonempty ray has a finite positive premultiplied distance");
    if (std::isfinite(sample.MaximumM)) {
      CHECK(moments.WeightedDistanceM <= sample.MaximumM * moments.Weight,
            "a depth-clipped cloud moment cannot move behind the receiving surface");
    }
  }
  CloudLayer layer{.BaseM = kCloudBaseM, .Cover = kBrokenCover};
  const Vec3 origin{{0.0, 1772.0, 0.0}};
  const Vec3 up{{0.0, 1.0, 0.0}};
  const auto blocked = TraceCloudMoments(layer, origin, up, {.MaximumM = 700.0});
  CHECK(blocked.Weight == 0.0 && blocked.WeightedDistanceM == 0.0,
        "geometry before a cloud produces the exact zero/zero filter identity");
  const auto ground = TraceCloudMoments(layer, origin, up * -1.0);
  CHECK(ground.Weight == 0.0 && ground.WeightedDistanceM == 0.0,
        "the opaque planet excludes the hidden cloud hemisphere from both moments");
  layer.Cover = 0.0;
  const auto clear = TraceCloudMoments(layer, origin, up);
  CHECK(clear.Weight == 0.0 && clear.WeightedDistanceM == 0.0,
        "clear weather stays zero/zero without constructing a fictitious depth");
  return Report();
}
