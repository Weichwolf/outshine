#ifndef OUTSHINE_TEST_DECLAREDWEATHERCAPTURE_H
#define OUTSHINE_TEST_DECLAREDWEATHERCAPTURE_H

#include "io/ReadTextFile.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>

namespace outshine::Test {
inline constexpr int64_t kDeclaredWeatherUtcS = 1792022400;
inline constexpr double kDeclaredWeatherLow = 0.65;

inline std::expected<std::string, std::string> DeclaredWeatherScenario() {
  return ReadTextFile("test/outshine/data/weather/declared-overcast.scenario", size_t{16} * 1024);
}
}
#endif
