#include "Check.h"
#include "WeatherCapture.h"
#include "LowCloud.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>

int main() {
  using namespace outshine;
  using namespace outshine::Generators::Weather;
  using namespace outshine::Test;
  const auto capture = RecordedWeatherCapture();
  CHECK(capture.has_value(), "versioned original response and receipt are available");
  if (!capture) { return Report(); }
  const OpenMeteoProvider provider(capture->Request);
  auto source = provider.make(capture->Declaration, {});
  CHECK(source.has_value(), "the public extension factory configures the captured request");
  if (!source) { return Report(); }
  auto snapshot = (*source)->DecodeWeather(WeatherBytes(*capture), kCapturedWeatherUtcS);
  CHECK(snapshot && snapshot->has_value(), "the public Source entry publishes the exact UTC");
  if (!snapshot || !*snapshot) { return Report(); }
  CHECK((*snapshot)->Values.ValidUtcS == kCapturedWeatherUtcS &&
            (*snapshot)->Values.CloudLow == 0.0 && (*snapshot)->Values.CloudHigh > 0.0,
        "known-zero low cloud remains independent of the actual high-cloud forecast");
  const auto &origin = (*snapshot)->Source;
  CHECK(origin.Origin == WeatherOrigin::ModelForecast && origin.Provider == "open-meteo" &&
            origin.SourceId == capture->Declaration.Dataset &&
            origin.SourceRevision == capture->Declaration.Revision &&
            origin.RequestUri == capture->Declaration.Endpoint &&
            origin.PayloadSha256 == capture->Declaration.PayloadSha256 &&
            origin.AcquiredUtcS == capture->Request.AcquiredUtcS && !origin.ModelRunUtcS &&
            origin.Requested.LatitudeDeg == capture->Request.Requested.LatitudeDeg &&
            origin.ModelGridLatitudeDeg &&
            origin.ModelGridLatitudeDeg != origin.Requested.LatitudeDeg &&
            origin.DownscalingSurfaceElevationAslM == kCapturedWeatherElevationAslM,
        "source-byte identity, acquisition, requested/grid position and downscaling stay distinct");
  CHECK(!(*snapshot)->Values.CloudBaseAglM && !(*snapshot)->Values.VerticalWindMs &&
            (*snapshot)->Values.VisibilityM == kCapturedWeatherVisibilityM &&
            (*snapshot)->Values.Precipitation.EndUtcS == kCapturedWeatherUtcS &&
            (*snapshot)->Values.Precipitation.BeginUtcS ==
                kCapturedWeatherUtcS - kWeatherSecondsPerHour,
        "unknown geometry stays missing, visibility retains metres and amounts retain their "
        "interval");
  for (const int64_t absent : {kCapturedWeatherUtcS + 1,
                               kCapturedWeatherUtcS - kWeatherSecondsPerHour,
                               kCapturedWeatherUtcS + kWeatherSecondsPerDay,
                               std::numeric_limits<int64_t>::min(),
                               std::numeric_limits<int64_t>::max()}) {
    const auto missing = (*source)->DecodeWeather(WeatherBytes(*capture), absent);
    CHECK(missing && !*missing, "an absent exact timestamp never selects a neighboring hour");
  }
  const auto clear = MakeLowCloud(**snapshot);
  CHECK(clear && *clear && !(**clear).ProceduralGeometry && !(**clear).ProceduralAdvection &&
            CloudOpticalDepth((**clear).Layer,
                              {{0.0, kCapturedWeatherElevationAslM, 0.0}},
                              {{0.0, 1.0, 0.0}}) == 0.0,
        "the actual low-cloud consumer yields transparent native extinction for this forecast");
  WeatherSnapshot declared;
  declared.Source.Origin = WeatherOrigin::ScenarioDeclaration;
  declared.Values.ValidUtcS = kCapturedWeatherUtcS;
  declared.Values.CloudLow = kDeclaredLowCover;
  CHECK(!MakeLowCloud(declared) && MakeLowCloud(declared).error() == LowCloudFailure::MissingRecipe,
        "positive declared cloud needs an explicit geometric and advection recipe");
  const LowCloudRecipe recipe{.BaseAslM = kDeclaredCloudBaseAslM,
                              .ThicknessM = kDeclaredCloudThicknessM,
                              .ExtinctionPerM = kDeclaredCloudExtinctionPerM,
                              .AdvectionEastMs = kDeclaredCloudAdvectionMs,
                              .AdvectionNorthMs = 0.0};
  const auto cloudy = MakeLowCloud(declared, recipe);
  CHECK(cloudy && *cloudy && (**cloudy).ProceduralGeometry && (**cloudy).ProceduralAdvection &&
            (**cloudy).ValidUtcS == declared.Values.ValidUtcS &&
            (**cloudy).Layer.WindMs[0] == kDeclaredCloudAdvectionMs &&
            (**cloudy).Layer.WindMs[2] == 0.0 &&
            CloudOpticalDepth((**cloudy).Layer,
                              {{0.0, kCapturedWeatherElevationAslM, 0.0}},
                              {{0.0, 1.0, 0.0}}) > 0.0,
        "a distinct declared case produces actual cloud extinction with labeled procedural inputs");
  CHECK(!declared.Values.CloudBaseAglM && !declared.Values.Wind10m &&
            declared.Source.Origin == WeatherOrigin::ScenarioDeclaration,
        "procedural completion neither mutates the source nor invents ten-metre wind");
  auto cumulusRecipe = recipe;
  cumulusRecipe.Profile = CloudProfile::CumulusV1;
  const auto cumulus = MakeLowCloud(declared, cumulusRecipe);
  CHECK(cumulus && *cumulus && (**cumulus).Layer.Profile == CloudProfile::CumulusV1 &&
            (**cumulus).ProceduralGeometry && (**cumulus).Layer.Cover == kDeclaredLowCover &&
            (**cloudy).Layer.Profile == CloudProfile::Stratus,
        "cloud shape is an explicit procedural recipe while old callers retain stratus");
  cumulusRecipe.Profile = static_cast<CloudProfile>(2);
  const auto unsupported = MakeLowCloud(declared, cumulusRecipe);
  CHECK(!unsupported && unsupported.error() == LowCloudFailure::InvalidRecipe,
        "unsupported recipe versions fail instead of silently changing shape");
  declared.Values.CloudLow.reset();
  const auto unknown = MakeLowCloud(declared, recipe);
  CHECK(unknown && !*unknown, "missing low-cloud cover is not filled from a recipe or total cover");
  const auto changed = capture->Bytes + "\n";
  const auto corrupt = (*source)->DecodeWeather(
      {reinterpret_cast<const uint8_t *>(changed.data()), changed.size()}, kCapturedWeatherUtcS);
  CHECK(!corrupt && corrupt.error() == Data::DecodeFailure::CorruptPayload,
        "changed original bytes fail the pinned digest before native publication");
  return Report();
}
