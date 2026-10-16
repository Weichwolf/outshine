#ifndef OUTSHINE_GENERATORS_WEATHER_LOWCLOUD_H
#define OUTSHINE_GENERATORS_WEATHER_LOWCLOUD_H

#include "CloudLayer.h"
#include <world/weather/WeatherSnapshot.h>

#include <expected>
#include <optional>

namespace outshine::Generators::Weather {

struct LowCloudRecipe {
  double BaseAslM;
  double ThicknessM;
  double ExtinctionPerM;
  double AdvectionEastMs;
  double AdvectionNorthMs;
  CloudProfile Profile = CloudProfile::Stratus;
};

enum class LowCloudFailure { InvalidCover, MissingRecipe, InvalidRecipe };

struct LowCloudState {
  CloudLayer Layer{};
  int64_t ValidUtcS = 0;
  bool ProceduralGeometry = false;
  bool ProceduralAdvection = false;
};

[[nodiscard]] std::expected<std::optional<LowCloudState>, LowCloudFailure>
MakeLowCloud(const WeatherSnapshot &snapshot,
             const std::optional<LowCloudRecipe> &recipe = std::nullopt) noexcept;

}
#endif
