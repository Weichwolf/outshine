#include "Check.h"
#include "WeatherForecastInput.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string_view>

int main() {
  using namespace outshine::Generators::Weather;
  using namespace outshine::Test;
  constexpr std::string_view payload =
      R"({"hourly_units":{"time":"unixtime"},"hourly":{"time":[3600]}})";
  const std::span bytes{reinterpret_cast<const uint8_t *>(payload.data()), payload.size()};
  const auto original = ForecastSourceFor(payload);
  const auto rejects = [&](const ForecastSource &source, ForecastFailure failure) {
    const auto result = DecodeOpenMeteo(bytes, source);
    return !result && result.error().Reason == failure;
  };
  auto source = original;
  source.PayloadSha256[0] = source.PayloadSha256[0] == '0' ? '1' : '0';
  CHECK(rejects(source, ForecastFailure::HashMismatch),
        "supplied digest must identify the actual payload bytes");
  source = original;
  constexpr uint16_t unavailable = 503;
  source.HttpStatus = unavailable;
  CHECK(rejects(source, ForecastFailure::InvalidSource), "unsuccessful HTTP is not a forecast");
  source = original;
  source.RequestUri = "https://example.org/v1/forecast?latitude=47&longitude=10";
  CHECK(rejects(source, ForecastFailure::InvalidSource),
        "another source cannot claim this decoder");
  source = original;
  constexpr double outsideLatitudeDeg = 91.0;
  source.Requested.LatitudeDeg = outsideLatitudeDeg;
  CHECK(rejects(source, ForecastFailure::InvalidSource),
        "invalid request coordinates are rejected");
  source = original;
  source.Requested.DeclaredSurfaceElevationAslM = std::numeric_limits<double>::quiet_NaN();
  CHECK(rejects(source, ForecastFailure::InvalidSource),
        "nonfinite acquisition height is rejected");
  source = original;
  source.AcquiredUtcS = std::nullopt;
  const auto decoded = DecodeOpenMeteo(bytes, source);
  CHECK(decoded && !decoded->Source.AcquiredUtcS && !decoded->ModelGridLatitudeDeg &&
            !decoded->DownscalingSurfaceElevationAslM,
        "unknown acquisition time and absent response position stay unknown");
  CHECK(decoded && decoded->Hours[0].Precipitation.BeginUtcS == 0 &&
            !decoded->Hours[0].Precipitation.TotalWaterKgM2,
        "the earliest sample owns its previous-hour interval without invented water");
  return Report();
}
