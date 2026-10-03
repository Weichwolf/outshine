#ifndef OUTSHINE_CLIENT_WORLDSOURCEPOLICY_H
#define OUTSHINE_CLIENT_WORLDSOURCEPOLICY_H

#include <scenario/Scenario.h>
#include <algorithm>
#include <expected>
#include <string>
#include "SourceProviderValidation.h"
#include "PreferredVectorSource.h"
#include "ShippedProviders.h"

namespace outshine::Client {

[[nodiscard]] inline std::expected<void, std::string>
ValidateWorldSources(const Scenario::Document &scenario) {
  if (scenario.Ground.Declared && scenario.Ground.Shape.Kind.empty() &&
      scenario.Ground.Osm.empty() &&
      !std::ranges::any_of(scenario.Providers, [](const Data::SourceProvider &provider) {
        return provider.Kind == "osm" || provider.Kind == "vector";
      })) {
    return std::unexpected("world data requires an explicit geographic vector source");
  }
  return {};
}

[[nodiscard]] inline std::expected<void, std::string>
ConfigureWorldSources(Scenario::Document &scenario) {
  if (scenario.Ground.Declared && scenario.Ground.Shape.Kind.empty() &&
      scenario.Ground.Osm.empty() &&
      !std::ranges::any_of(scenario.Providers, [](const Data::SourceProvider &provider) {
        return provider.Kind == "osm" || provider.Kind == "vector";
      })) {
    scenario.Providers.push_back(Generators::Osm::PreferredVectorSource());
  }
  if (scenario.Ground.Declared && scenario.Ground.Shape.Kind.empty()) {
    for (const auto &defaults : Generators::ShippedProviders()) {
      if (std::ranges::any_of(scenario.Providers, [&](const auto &provider) {
            return provider.Kind == defaults.Kind ||
                   (defaults.Kind == "terrain" && provider.Kind == "copernicus");
          })) {
        continue;
      }
      scenario.Providers.push_back(defaults);
    }
  }
  return ValidateWorldSources(scenario);
}

}
#endif
