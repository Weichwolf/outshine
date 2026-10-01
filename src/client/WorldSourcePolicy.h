#ifndef OUTSHINE_CLIENT_WORLDSOURCEPOLICY_H
#define OUTSHINE_CLIENT_WORLDSOURCEPOLICY_H

#include <scenario/Scenario.h>
#include <algorithm>
#include <expected>
#include <string>
#include "SourceProviderValidation.h"

namespace outshine::Client {

[[nodiscard]] inline std::expected<void, std::string>
ValidateWorldSources(const Scenario::Document &scenario) {
  if (std::ranges::any_of(scenario.Providers, [](const Data::SourceProvider &provider) {
        return provider.Kind == "vector";
      })) {
    return std::unexpected(
        "world data requires original OSM; vector-map tile providers are not permitted");
  }
  if (scenario.Ground.Declared && scenario.Ground.Shape.Kind.empty() &&
      scenario.Ground.Osm.empty() &&
      !std::ranges::any_of(scenario.Providers, [](const Data::SourceProvider &provider) {
        return provider.Kind == "osm";
      })) {
    return std::unexpected(
        "world data requires an official original OSM source; no map-tile fallback exists");
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
    scenario.Providers.push_back({.Kind = "osm",
                                  .Revision = "current",
                                  .Missing = Data::MissingDataPolicy::Fail,
                                  .Dataset = "openstreetmap.original",
                                  .Location = {},
                                  .Endpoint = std::string(Data::kOfficialOsmApi),
                                  .Coverage = {},
                                  .PayloadSha256 = {}});
  }
  return ValidateWorldSources(scenario);
}

}
#endif
