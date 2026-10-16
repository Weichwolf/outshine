#ifndef OUTSHINE_TEST_DECLAREDWEATHERCAPTURE_H
#define OUTSHINE_TEST_DECLAREDWEATHERCAPTURE_H

#include <cstdint>
#include <string_view>

namespace outshine::Test {
inline constexpr int64_t kDeclaredWeatherUtcS = 1792022400;
inline constexpr double kDeclaredWeatherLow = 0.65;
inline constexpr std::string_view kDeclaredWeatherScenario =
    R"(<scenario name="native-declared-weather">
  <clock start="2026-10-15T00:00:00Z"/>
  <weather validUtcS="1792022400" latitudeDeg="47.25" longitudeDeg="10.25"
      declaredSurfaceElevationAslM="1772" cloudTotal="1" cloudLow="0.65" cloudMid="0"
      cloudHigh="1" windSpeed10mMs="8" windFrom10mDeg="270" airTemperature2mK="288.15"
      relativeHumidity2m="0.7" visibilityM="12000" surfacePressurePa="84000"
      snowDepthM="0" cloudBaseAglM="800" verticalWindMs="-0.5">
    <wind10m eastMs="8" northMs="0"/>
    <precipitation beginUtcS="1792018800" endUtcS="1792022400" totalWaterKgM2="2.5"
        rainWaterKgM2="2" showersWaterKgM2="0.5" snowfallDepthM="0"/>
  </weather>
</scenario>)";
}
#endif
