#ifndef OUTSHINE_SCENARIO_WEATHERDECLARATION_H
#define OUTSHINE_SCENARIO_WEATHERDECLARATION_H

#include <world/weather/WeatherSnapshot.h>

namespace outshine::Scenario {

/// Owned explicit weather input, independent of terrain, provider data and legacy sky styling.
/// Missing optional values remain unknown; zero remains known zero. Declaring it alongside a
/// weather provider is ambiguous and rejected. This value performs no IO or synchronization.
struct WeatherDeclaration {
  /// Declared geodetic anchor; a supplied surface elevation remains request metadata.
  WeatherPosition At{};
  /// Native units and sampling heights; ValidUtcS must equal the resolved initial clock UTC.
  /// No time interpolation, field completion or conversion into surface wetness is implicit.
  WeatherValues Values{};
};

}
#endif
