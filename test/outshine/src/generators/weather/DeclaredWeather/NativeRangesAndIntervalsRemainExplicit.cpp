#include "DeclaredWeather.h"
#include "Check.h"

#include <array>
#include <cstdint>
#include <limits>
#include <optional>

int main() {
  using namespace outshine;
  using namespace outshine::Generators::Weather;
  using namespace outshine::Test;
  constexpr int64_t kHourS = 3600;
  constexpr double kFullBearingDeg = 360.0;
  WeatherValues values;
  const WeatherPosition at;
  const auto unknown = MakeDeclaredWeather(at, values, 0);
  CHECK(unknown && !unknown->Values.CloudLow && !unknown->Values.WindSpeed10mMs,
        "an explicit all-unknown declaration does not become known zeros");
  const auto wrongTime = MakeDeclaredWeather(at, values, 1);
  CHECK(!wrongTime && wrongTime.error() == WeatherInputFailure::InvalidUtc,
        "the exact resolved UTC cannot silently replace the supplied validity time");
  values.Precipitation = {.BeginUtcS = -kHourS, .EndUtcS = 0, .RainWaterKgM2 = 0.0};
  const auto zeroRain = MakeDeclaredWeather(at, values, 0);
  CHECK(zeroRain && zeroRain->Values.Precipitation.RainWaterKgM2 == 0.0 &&
            zeroRain->Values.Precipitation.BeginUtcS == -kHourS &&
            !zeroRain->Values.Precipitation.SnowfallDepthM,
        "known-zero rain retains its preceding interval without inventing snowfall");
  values.Precipitation.EndUtcS = 1;
  const auto futureRain = MakeDeclaredWeather(at, values, 0);
  CHECK(!futureRain && futureRain.error() == WeatherInputFailure::InvalidInterval,
        "a following precipitation interval is not accepted as a preceding amount");
  values.Precipitation.BeginUtcS = 0;
  values.Precipitation.EndUtcS = 0;
  const auto noInterval = MakeDeclaredWeather(at, values, 0);
  CHECK(!noInterval && noInterval.error() == WeatherInputFailure::InvalidInterval,
        "even a known-zero amount needs an explicit nonempty interval");
  values.Precipitation = {};
  const std::array fractions{&WeatherValues::CloudTotal,
                             &WeatherValues::CloudLow,
                             &WeatherValues::CloudMid,
                             &WeatherValues::CloudHigh,
                             &WeatherValues::RelativeHumidity2m};
  for (const auto member : fractions) {
    for (const double invalid : {-1.0, 2.0, std::numeric_limits<double>::quiet_NaN()}) {
      auto broken = values;
      broken.*member = invalid;
      const auto result = MakeDeclaredWeather(at, broken, 0);
      CHECK(!result && result.error() == WeatherInputFailure::InvalidValues,
            "each optional fraction rejects negative, excessive and nonfinite inputs");
    }
  }
  values.WindFrom10mDeg = kFullBearingDeg;
  CHECK(!MakeDeclaredWeather(at, values, 0), "native bearings must already be canonical");
  values.WindFrom10mDeg.reset();
  values.Wind10m =
      WeatherWind10m{.EastMs = 0.0, .NorthMs = std::numeric_limits<double>::infinity()};
  CHECK(!MakeDeclaredWeather(at, values, 0), "native ENU wind must be finite");
  values.Wind10m.reset();
  values.ValidUtcS = std::numeric_limits<int64_t>::min() + 1;
  values.Precipitation = {.BeginUtcS = std::numeric_limits<int64_t>::min(),
                          .EndUtcS = values.ValidUtcS,
                          .SnowfallDepthM = 0.0};
  CHECK(MakeDeclaredWeather(at, values, values.ValidUtcS).has_value(),
        "signed UTC bounds are compared directly without interval subtraction overflow");
  auto belowSea = at;
  belowSea.DeclaredSurfaceElevationAslM = -1.0;
  CHECK(MakeDeclaredWeather(belowSea, values, values.ValidUtcS).has_value(),
        "an explicitly supplied below-sea-level position remains valid");
  belowSea.LatitudeDeg = std::numeric_limits<double>::infinity();
  CHECK(!MakeDeclaredWeather(belowSea, values, values.ValidUtcS),
        "the declaration cannot publish a nonfinite geodetic position");
  return Report();
}
