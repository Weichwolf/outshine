#include "Check.h"
#include "WeatherForecastInput.h"
#include "math/RenderFrame.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <string_view>

int main() {
  using namespace outshine::Generators::Weather;
  using namespace outshine::Test;
  constexpr std::string_view payload = R"({
    "hourly_units":{"time":"unixtime","wind_speed_10m":"m/s","wind_direction_10m":"°"},
    "hourly":{"time":[3600,7200,10800,14400],"wind_speed_10m":[8,8,8,8],
      "wind_direction_10m":[0,90,180,270]}})";
  constexpr std::array expected = {HorizontalWind10m{.EastMs = 0.0, .NorthMs = -8.0},
                                   HorizontalWind10m{.EastMs = -8.0, .NorthMs = 0.0},
                                   HorizontalWind10m{.EastMs = 0.0, .NorthMs = 8.0},
                                   HorizontalWind10m{.EastMs = 8.0, .NorthMs = 0.0}};
  constexpr std::array rendered = {std::array{0.0, 0.0, 8.0},
                                   std::array{-8.0, 0.0, 0.0},
                                   std::array{0.0, 0.0, -8.0},
                                   std::array{8.0, 0.0, 0.0}};
  constexpr double epsilon = 1e-12;
  const auto decoded = DecodeForecastFor(payload);
  CHECK(decoded && decoded->Hours.size() == expected.size(), "all cardinal source records decode");
  if (!decoded || decoded->Hours.size() != expected.size()) { return Report(); }
  for (size_t index = 0; index < expected.size(); ++index) {
    const auto &wind = decoded->Hours[index].Wind10m;
    CHECK(wind && std::abs(wind->EastMs - expected[index].EastMs) < epsilon &&
              std::abs(wind->NorthMs - expected[index].NorthMs) < epsilon,
          "wind FROM each cardinal bearing flows to the opposite direction in ENU");
    if (!wind) { continue; }
    const auto graphics =
        outshine::RenderFrame::Of({.EastM = wind->EastMs, .NorthM = wind->NorthMs, .UpM = 0.0});
    CHECK(std::abs(graphics[0] - rendered[index][0]) < epsilon &&
              std::abs(graphics[1] - rendered[index][1]) < epsilon &&
              std::abs(graphics[2] - rendered[index][2]) < epsilon,
          "the existing RenderFrame performs the only conversion of horizontal ENU to rendering");
  }
  return Report();
}
