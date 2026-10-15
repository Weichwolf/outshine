#include "Check.h"
#include "WeatherForecastInput.h"

#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>

int main() {
  using namespace outshine::Generators::Weather;
  using namespace outshine::Test;
  constexpr double expectedGridLatitudeDeg = 47.25;
  constexpr double expectedDownscalingElevationM = 1000.0;
  constexpr int64_t firstUtcS = 2208988800;
  constexpr int64_t offsetS = 7200;
  constexpr double epsilon = 1e-12;
  constexpr double freezingK = 273.15;
  constexpr double expectedPressurePa = 90000.0;
  constexpr double expectedSnowfallM = 0.02;
  constexpr double expectedSnowpackM = 0.3;
  constexpr std::string_view payload = R"({
    "latitude":47.25,"longitude":10.125,"elevation":1000,"utc_offset_seconds":7200,
    "hourly_units":{"time":"unixtime","cloud_cover":"%","cloud_cover_low":"%",
      "cloud_cover_high":"%","wind_speed_10m":"m/s","wind_direction_10m":"°",
      "temperature_2m":"°C","relative_humidity_2m":"%","surface_pressure":"hPa",
      "precipitation":"mm","rain":"mm","snowfall":"cm","snow_depth":"m"},
    "hourly":{"time":[2208988800,2208992400],"cloud_cover":[100,null],
      "cloud_cover_low":[0,null],"cloud_cover_high":[100,0],"wind_speed_10m":[4,0],
      "wind_direction_10m":[90,null],"temperature_2m":[0,-10],
      "relative_humidity_2m":[50,0],"surface_pressure":[900,0],
      "precipitation":[1,0],"rain":[0,null],"snowfall":[2,0],"snow_depth":[0.3,0]}})";
  const auto decoded = DecodeForecastFor(payload);
  CHECK(decoded.has_value(), "explicit hourly forecast normalizes without IO");
  if (!decoded) { return Report(); }
  CHECK(decoded->Hours.size() == 2, "both hourly model records are retained");
  CHECK(decoded->Source.PayloadSha256 == ForecastSourceFor(payload).PayloadSha256 &&
            decoded->Source.RequestUri == ForecastSourceFor(payload).RequestUri,
        "actual source digest and acquisition identity remain owned");
  CHECK(decoded->Source.Requested.LatitudeDeg == kForecastRequestLatitudeDeg &&
            decoded->ModelGridLatitudeDeg == expectedGridLatitudeDeg,
        "requested and returned model-grid positions are separate");
  CHECK(decoded->Source.Requested.DeclaredSurfaceElevationAslM == kForecastRequestElevationM &&
            decoded->DownscalingSurfaceElevationAslM == expectedDownscalingElevationM,
        "returned downscaling surface is not camera or requested height");
  CHECK(decoded->SourceUtcOffsetS == offsetS && decoded->Hours[0].ValidUtcS == firstUtcS,
        "post-2038 UTC remains int64 and ignores the response timezone offset");
  const auto *hour = ExactHour(*decoded, firstUtcS);
  CHECK(hour != nullptr && ExactHour(*decoded, firstUtcS + 1) == nullptr &&
            ExactHour(*decoded, firstUtcS - 1) == nullptr &&
            ExactHour(*decoded, firstUtcS + 2 * kWeatherHourS) == nullptr,
        "native consumer selects only an exact available UTC hour");
  if (hour == nullptr) { return Report(); }
  CHECK(hour->CloudTotal == 1.0 && hour->CloudLow == 0.0 && hour->CloudHigh == 1.0 &&
            !hour->CloudMid,
        "high cloud cover never creates an observed low cloud deck");
  CHECK(hour->Wind10m && std::abs(hour->Wind10m->EastMs + 4.0) < epsilon &&
            std::abs(hour->Wind10m->SouthMs) < epsilon,
        "meteorological east wind becomes westward flow at its stated 10m height");
  CHECK(hour->AirTemperature2mK == freezingK && hour->RelativeHumidity2m == 0.5 &&
            hour->SurfacePressurePa == expectedPressurePa,
        "temperature, humidity and pressure use their declared native units");
  CHECK(hour->Precipitation.BeginUtcS == hour->ValidUtcS - kWeatherHourS &&
            hour->Precipitation.EndUtcS == hour->ValidUtcS &&
            hour->Precipitation.TotalWaterKgM2 == 1.0 && hour->Precipitation.RainWaterKgM2 == 0.0 &&
            !hour->Precipitation.ShowersWaterKgM2,
        "amounts belong to the preceding hour, with known zero distinct from unknown");
  CHECK(hour->Precipitation.SnowfallDepthM == expectedSnowfallM &&
            hour->SnowDepthM == expectedSnowpackM,
        "new snowfall depth, snow water amount and snowpack depth remain separate");
  CHECK(!hour->CloudBaseAglM && !hour->VerticalWindMs && !hour->VisibilityM &&
            !decoded->Source.ModelRunUtcS,
        "absent cloud base, vertical wind, visibility and model run are never fabricated");
  const auto &later = decoded->Hours[1];
  CHECK(!later.CloudTotal && !later.CloudLow && later.CloudHigh == 0.0 && later.Wind10m &&
            later.Wind10m->EastMs == 0.0 && later.Wind10m->SouthMs == 0.0,
        "null cloud values remain unknown; known calm does not require a bearing");
  return Report();
}
