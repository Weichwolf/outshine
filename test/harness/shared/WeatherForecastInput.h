#ifndef OUTSHINE_TEST_WEATHERFORECASTINPUT_H
#define OUTSHINE_TEST_WEATHERFORECASTINPUT_H

#include "OpenMeteo.h"
#include "format/Sha256.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace outshine::Test {

inline constexpr double kForecastRequestLatitudeDeg = 47.0;
inline constexpr double kForecastRequestLongitudeDeg = 10.0;
inline constexpr double kForecastRequestElevationM = 1772.0;
inline constexpr int64_t kForecastAcquiredUtcS = 1792066383;
inline constexpr uint16_t kForecastHttpOk = 200;

inline Generators::Weather::ForecastSource ForecastSourceFor(std::string_view bytes) {
  return {.Requested = {.LatitudeDeg = kForecastRequestLatitudeDeg,
                        .LongitudeDeg = kForecastRequestLongitudeDeg,
                        .DeclaredSurfaceElevationAslM = kForecastRequestElevationM},
          .RequestUri = "https://api.open-meteo.com/v1/"
                        "forecast?latitude=47&longitude=10&elevation=1772&timeformat=unixtime",
          .PayloadSha256 = Sha256Hex(bytes),
          .AcquiredUtcS = kForecastAcquiredUtcS,
          .ModelRunUtcS = std::nullopt,
          .RequestedModels = std::nullopt,
          .HttpStatus = kForecastHttpOk};
}

inline auto DecodeForecastFor(std::string_view bytes) {
  return Generators::Weather::DecodeOpenMeteo(
      {reinterpret_cast<const uint8_t *>(bytes.data()), bytes.size()}, ForecastSourceFor(bytes));
}

}
#endif
