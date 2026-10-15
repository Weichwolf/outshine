#include "OpenMeteo.h"
#include "OpenMeteoFields.h"
#include "format/Json.h"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <iterator>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

namespace {
using namespace outshine;
using namespace outshine::Generators::Weather;
constexpr uint16_t kHttpOk = 200;

std::string Read(const char *path) {
  std::ifstream file(path, std::ios::binary);
  std::string data;
  for (char byte = 0; data.size() <= kOpenMeteoMaximumBytes && file.get(byte);) {
    data.push_back(byte);
  }
  return data;
}

std::optional<ForecastSource> Context(std::string_view text) {
  Json doc;
  if (!doc.Parse(text.data(), text.size())) { return std::nullopt; }
  const auto root = doc.Root();
  if (root["latitude_deg"].GetKind() != Json::Kind::Number ||
      root["longitude_deg"].GetKind() != Json::Kind::Number) {
    return std::nullopt;
  }
  ForecastSource result;
  result.Requested.LatitudeDeg = root["latitude_deg"].Num();
  result.Requested.LongitudeDeg = root["longitude_deg"].Num();
  result.RequestUri = root["url"].Str();
  result.PayloadSha256 = root["source_sha256"].Str();
  result.HttpStatus = root["http_status"].Int() == kHttpOk ? kHttpOk : 0;
  if (root["elevation_asl_m"].GetKind() != Json::Kind::Null) {
    if (root["elevation_asl_m"].GetKind() != Json::Kind::Number) { return std::nullopt; }
    result.Requested.DeclaredSurfaceElevationAslM = root["elevation_asl_m"].Num();
  }
  if (root["acquired_utc_s"].GetKind() != Json::Kind::Null) {
    const auto at = WeatherInteger(root["acquired_utc_s"], "acquired_utc_s");
    if (!at) { return std::nullopt; }
    result.AcquiredUtcS = *at;
  }
  return result;
}

template <typename T> void Number(const std::optional<T> &value) {
  if (!value) {
    std::print("null");
  } else if constexpr (std::is_floating_point_v<T>) {
    std::print("{:.17g}", *value);
  } else {
    std::print("{}", *value);
  }
}

void Hour(const HourlyWeather &hour) {
  std::print("[{}", hour.ValidUtcS);
  constexpr std::array members = {&HourlyWeather::CloudTotal,
                                  &HourlyWeather::CloudLow,
                                  &HourlyWeather::CloudMid,
                                  &HourlyWeather::CloudHigh,
                                  &HourlyWeather::WindSpeed10mMs,
                                  &HourlyWeather::WindFrom10mDeg};
  for (const auto member : members) {
    std::print(",");
    Number(hour.*member);
  }
  std::print(",");
  Number(hour.Wind10m ? std::optional{hour.Wind10m->EastMs} : std::nullopt);
  std::print(",");
  Number(hour.Wind10m ? std::optional{hour.Wind10m->SouthMs} : std::nullopt);
  constexpr std::array later = {&HourlyWeather::AirTemperature2mK,
                                &HourlyWeather::RelativeHumidity2m,
                                &HourlyWeather::VisibilityM,
                                &HourlyWeather::SurfacePressurePa,
                                &HourlyWeather::SnowDepthM,
                                &HourlyWeather::CloudBaseAglM,
                                &HourlyWeather::VerticalWindMs};
  for (const auto member : later) {
    std::print(",");
    Number(hour.*member);
  }
  std::print(",{},{}", hour.Precipitation.BeginUtcS, hour.Precipitation.EndUtcS);
  constexpr std::array amounts = {&PreviousHour::TotalWaterKgM2,
                                  &PreviousHour::RainWaterKgM2,
                                  &PreviousHour::ShowersWaterKgM2,
                                  &PreviousHour::SnowfallDepthM};
  for (const auto member : amounts) {
    std::print(",");
    Number(hour.Precipitation.*member);
  }
  std::print("]");
}
}

int main(int argc, char **argv) {
  if (argc != 3 && argc != 4) {
    std::println(stderr, "usage: native-forecast payload.json request-context.json [exact-utc-s]");
    return 2;
  }
  const auto input = Read(argv[1]);
  const auto context = Context(Read(argv[2]));
  if (!context) { return 2; }
  const auto forecast =
      DecodeOpenMeteo({reinterpret_cast<const uint8_t *>(input.data()), input.size()}, *context);
  if (!forecast) {
    std::println(stderr,
                 "native forecast error={} field={} index={}",
                 static_cast<int>(forecast.error().Reason),
                 forecast.error().Field,
                 forecast.error().Index);
    return 2;
  }
  const HourlyWeather *selected = nullptr;
  if (argc == 4) {
    int64_t utcS = 0;
    const std::string_view text{argv[3]};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), utcS);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) { return 2; }
    selected = ExactHour(*forecast, utcS);
    if (selected == nullptr) { return 3; }
  }
  std::print(R"({{"source_sha256":"{}","request":[{:.17g},{:.17g},)",
             forecast->Source.PayloadSha256,
             forecast->Source.Requested.LatitudeDeg,
             forecast->Source.Requested.LongitudeDeg);
  Number(forecast->Source.Requested.DeclaredSurfaceElevationAslM);
  std::print("],\"grid\":[");
  Number(forecast->ModelGridLatitudeDeg);
  std::print(",");
  Number(forecast->ModelGridLongitudeDeg);
  std::print(",");
  Number(forecast->DownscalingSurfaceElevationAslM);
  std::print("],\"utc_offset\":");
  Number(forecast->SourceUtcOffsetS);
  std::print(",\"acquired_utc\":");
  Number(forecast->Source.AcquiredUtcS);
  std::print(",\"model_run_utc\":");
  Number(forecast->Source.ModelRunUtcS);
  std::print(",\"hours\":[");
  if (selected != nullptr) {
    Hour(*selected);
  } else {
    for (size_t index = 0; index < forecast->Hours.size(); ++index) {
      if (index != 0) { std::print(","); }
      Hour(forecast->Hours[index]);
    }
  }
  std::println("]}}");
  return 0;
}
