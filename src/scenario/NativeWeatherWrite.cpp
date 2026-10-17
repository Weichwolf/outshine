#include "NativeWeather.h"
#include "NativeWeatherFields.h"

#include <format>
#include <optional>
#include <string>

namespace outshine {
namespace {
void Number(std::string &into, const char *name, const std::optional<double> &value) {
  if (value) { into += std::format(" {}=\"{}\"", name, *value); }
}

void Precipitation(std::string &into, const WeatherPrecipitation &rain) {
  bool given = rain.BeginUtcS != 0 || rain.EndUtcS != 0;
  for (const auto &field : kNativePrecipitationFields) {
    given = given || (rain.*field.Member).has_value();
  }
  if (!given) { return; }
  into += std::format(
      R"(    <precipitation beginUtcS="{}" endUtcS="{}")", rain.BeginUtcS, rain.EndUtcS);
  for (const auto &field : kNativePrecipitationFields) {
    Number(into, field.Name, rain.*field.Member);
  }
  into += "/>\n";
}
}

void WriteNativeWeather(std::string &into,
                        const std::optional<Scenario::WeatherDeclaration> &weather) {
  if (!weather) { return; }
  const auto &values = weather->Values;
  into += std::format(R"(  <weather validUtcS="{}" latitudeDeg="{}" longitudeDeg="{}")",
                      values.ValidUtcS,
                      weather->At.LatitudeDeg,
                      weather->At.LongitudeDeg);
  Number(into, "declaredSurfaceElevationAslM", weather->At.DeclaredSurfaceElevationAslM);
  for (const auto &field : kNativeWeatherFields) { Number(into, field.Name, values.*field.Member); }
  into += ">\n";
  if (values.Wind10m) {
    into += std::format("    <wind10m eastMs=\"{}\" northMs=\"{}\"/>\n",
                        values.Wind10m->EastMs,
                        values.Wind10m->NorthMs);
  }
  Precipitation(into, values.Precipitation);
  into += "  </weather>\n";
}
}
