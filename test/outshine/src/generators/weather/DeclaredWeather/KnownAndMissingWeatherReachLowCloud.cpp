#include "DeclaredWeather.h"
#include "LowCloud.h"
#include "Check.h"

#include <cstdint>

int main() {
  using namespace outshine;
  using namespace outshine::Generators::Weather;
  using namespace outshine::Test;
  constexpr int64_t kUtcS = 1792022400;
  constexpr double kLatitudeDeg = 47.25;
  constexpr double kLongitudeDeg = 10.25;
  constexpr double kCover = 0.65;
  constexpr double kCloudBaseAslM = 2572.0;
  constexpr double kEyeAslM = 1772.0;
  WeatherPosition at{.LatitudeDeg = kLatitudeDeg, .LongitudeDeg = kLongitudeDeg};
  WeatherValues values{.ValidUtcS = kUtcS, .CloudTotal = 1.0, .CloudHigh = 1.0};
  const auto missing = MakeDeclaredWeather(at, values, kUtcS);
  CHECK(missing && !missing->Values.CloudLow &&
            missing->Source.Origin == WeatherOrigin::ScenarioDeclaration &&
            missing->Source.Requested.LatitudeDeg == kLatitudeDeg &&
            !missing->Source.DownscalingSurfaceElevationAslM &&
            missing->Source.PayloadSha256.empty() && missing->Source.Provider.empty(),
        "native declarations retain missing low cloud and assert no model or fetched bytes");
  if (!missing) { return Report(); }
  const auto absentLayer = MakeLowCloud(*missing);
  CHECK(absentLayer && !*absentLayer,
        "declared total/high cover cannot replace missing native low cover");
  values.CloudLow = 0.0;
  const auto clear = MakeDeclaredWeather(at, values, kUtcS);
  CHECK(clear && clear->Values.CloudLow == 0.0, "known zero remains a present native value");
  if (!clear) { return Report(); }
  const auto transparent = MakeLowCloud(*clear);
  CHECK(transparent && *transparent && (**transparent).Layer.Cover == 0.0 &&
            !(**transparent).ProceduralGeometry,
        "known-zero declared low cloud produces a transparent layer without a recipe");
  values.CloudLow = kCover;
  values.WindSpeed10mMs = 0.0;
  values.WindFrom10mDeg = 0.0;
  const auto overcast = MakeDeclaredWeather(at, values, kUtcS);
  CHECK(overcast && !overcast->Values.Wind10m && !overcast->Values.VerticalWindMs &&
            !overcast->Values.VisibilityM && !overcast->Values.Precipitation.RainWaterKgM2,
        "the native input does not fill missing ENU, vertical wind, visibility or rain");
  if (!overcast) { return Report(); }
  const auto needsRecipe = MakeLowCloud(*overcast);
  CHECK(!needsRecipe && needsRecipe.error() == LowCloudFailure::MissingRecipe,
        "declared coverage does not manufacture cloud geometry or cloud-height wind");
  const LowCloudRecipe recipe{.BaseAslM = kCloudBaseAslM,
                              .ThicknessM = kCloudLayerThicknessM,
                              .ExtinctionPerM = kCloudExtinctionPerM,
                              .AdvectionEastMs = 0.0,
                              .AdvectionNorthMs = 0.0};
  const auto layer = MakeLowCloud(*overcast, recipe);
  CHECK(layer && *layer && (**layer).ProceduralGeometry && (**layer).ProceduralAdvection &&
            CloudOpticalDepth((**layer).Layer, {{0.0, kEyeAslM, 0.0}}, {{0.0, 1.0, 0.0}}) > 0.0,
        "the declared native values drive the existing explicit low-cloud consumer");
  at.LatitudeDeg = 0.0;
  values.CloudLow.reset();
  CHECK(overcast->Values.CloudLow == kCover &&
            overcast->Source.Requested.LatitudeDeg == kLatitudeDeg,
        "the publication owns values and position independently of mutable declaration inputs");
  return Report();
}
