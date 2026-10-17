#include "NativeWeather.h"
#include "NativeWeatherFields.h"
#include "WeatherInput.h"
#include "Number.h"
#include "Xml.h"

#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <system_error>

namespace outshine {
namespace {
bool Integer(const Xml::Ref &from, const char *name, int64_t &into, std::string &error) {
  const auto token = from.Said(name);
  if (token) {
    const auto parsed = std::from_chars(token->data(), token->data() + token->size(), into);
    if (parsed.ec == std::errc{} && parsed.ptr == token->data() + token->size()) { return true; }
  }
  error = std::string("native weather requires a complete signed UTC integer: ") + name;
  return false;
}

bool Number(const Xml::Ref &from,
            const char *name,
            std::optional<double> &into,
            std::string &error) {
  const auto token = from.Said(name);
  if (!token) { return true; }
  const auto parsed = ParseFiniteNumber(*token);
  if (!parsed) {
    error = std::string("invalid native weather number: ") + name;
    return false;
  }
  into = *parsed;
  return true;
}

bool RequiredNumber(const Xml::Ref &from, const char *name, double &into, std::string &error) {
  std::optional<double> value;
  if (!Number(from, name, value, error)) { return false; }
  if (!value) {
    error = std::string("native weather requires: ") + name;
    return false;
  }
  into = *value;
  return true;
}

bool Precipitation(const Xml::Ref &from, WeatherPrecipitation &into, std::string &error) {
  if (!from.Valid()) { return true; }
  if (!Integer(from, "beginUtcS", into.BeginUtcS, error) ||
      !Integer(from, "endUtcS", into.EndUtcS, error)) {
    return false;
  }
  for (const auto &field : kNativePrecipitationFields) {
    if (!Number(from, field.Name, into.*field.Member, error)) { return false; }
  }
  return true;
}
}

bool ReadNativeWeather(const Xml::Ref &from,
                       std::optional<Scenario::WeatherDeclaration> &into,
                       std::string &error) {
  if (!from.Valid()) { return true; }
  Scenario::WeatherDeclaration weather;
  if (!Integer(from, "validUtcS", weather.Values.ValidUtcS, error) ||
      !RequiredNumber(from, "latitudeDeg", weather.At.LatitudeDeg, error) ||
      !RequiredNumber(from, "longitudeDeg", weather.At.LongitudeDeg, error) ||
      !Number(
          from, "declaredSurfaceElevationAslM", weather.At.DeclaredSurfaceElevationAslM, error)) {
    return false;
  }
  for (const auto &field : kNativeWeatherFields) {
    if (!Number(from, field.Name, weather.Values.*field.Member, error)) { return false; }
  }
  if (const auto wind = from.Child("wind10m"); wind.Valid()) {
    WeatherWind10m velocity;
    if (!RequiredNumber(wind, "eastMs", velocity.EastMs, error) ||
        !RequiredNumber(wind, "northMs", velocity.NorthMs, error)) {
      return false;
    }
    weather.Values.Wind10m = velocity;
  }
  if (!Precipitation(from.Child("precipitation"), weather.Values.Precipitation, error)) {
    return false;
  }
  if (const auto valid = ValidateWeatherInput(weather.At, weather.Values, weather.Values.ValidUtcS);
      !valid) {
    error = WeatherInputMessage(valid.error());
    return false;
  }
  into = weather;
  return true;
}
}
