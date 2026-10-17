#include "CloudFrame.h"
#include "Check.h"

#include <array>
#include <cmath>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Generators::Weather;
  using namespace outshine::Test;
  constexpr double kInfinity = std::numeric_limits<double>::infinity();
  constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
  const CloudLayer original;
  const std::array members{&CloudLayer::RadiusM,
                           &CloudLayer::BaseM,
                           &CloudLayer::ThicknessM,
                           &CloudLayer::ExtinctionPerM,
                           &CloudLayer::Cover};
  for (const auto member : members) {
    for (const double value : {kNaN, kInfinity, -kInfinity, -1.0}) {
      auto broken = original;
      broken.*member = value;
      const auto result = MakeCloudFrame(broken, 0.0);
      CHECK(!result && result.error() == CloudFrameFailure::InvalidLayer,
            "nonphysical and nonfinite shell/coverage values are rejected before packing");
    }
  }
  for (const double value : {kNaN, kInfinity, -kInfinity}) {
    auto broken = original;
    broken.WindMs[0] = value;
    CHECK(!MakeCloudFrame(broken, 0.0), "nonfinite wind cannot enter multiplication");
    const auto result = MakeCloudFrame(original, 0.0, {{value, 0.0, 0.0}});
    CHECK(!result && result.error() == CloudFrameFailure::InvalidOrigin,
          "nonfinite anchors cannot be converted into float payloads");
  }
  auto zeroThickness = original;
  zeroThickness.ThicknessM = 0.0;
  CHECK(!MakeCloudFrame(zeroThickness, 0.0), "an empty physical shell is rejected");
  auto tinyThickness = original;
  tinyThickness.ThicknessM = std::numeric_limits<double>::denorm_min();
  const auto tiny = MakeCloudFrame(tinyThickness, 0.0);
  CHECK(!tiny && tiny.error() == CloudFrameFailure::Unrepresentable,
        "positive thickness cannot silently underflow to an empty GPU shell");
  const auto enormous =
      MakeCloudFrame(original, 0.0, {{std::numeric_limits<double>::max(), 0.0, 0.0}});
  CHECK(!enormous && enormous.error() == CloudFrameFailure::Unrepresentable,
        "finite out-of-float-range anchors are rejected before conversion");
  auto moving = original;
  moving.WindMs[0] = 1.0;
  constexpr double kCellM = 1400.0;
  const auto carry = MakeCloudFrame(moving, -std::nextafter(kCellM, 0.0));
  CHECK(carry && carry->BroadBase[0] == 1 && carry->BroadFraction[0] == 0.0f,
        "a fractional value rounding to one carries into the integer lattice");
  const auto wrapped = MakeCloudFrame(moving, std::numeric_limits<double>::epsilon());
  CHECK(wrapped && wrapped->BroadBase[0] == 0 && wrapped->BroadFraction[0] == 0.0f,
        "negative near-zero carry uses defined unsigned wrap rather than signed overflow");
  return Report();
}
