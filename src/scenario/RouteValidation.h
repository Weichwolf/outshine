#ifndef OUTSHINE_SCENARIO_ROUTEVALIDATION_H
#define OUTSHINE_SCENARIO_ROUTEVALIDATION_H

#include <cstddef>
#include <expected>
#include <span>
#include <string>

#include <scenario/Scenario.h>

namespace outshine {

[[nodiscard]] inline std::expected<void, std::string>
ValidateRouteDeclarations(std::span<const Scenario::RouteDeclaration> routes) {
  constexpr size_t kMaxRoutes = 32;
  constexpr size_t kMaxNameBytes = 64;
  if (routes.size() > kMaxRoutes) { return std::unexpected("scenario exceeds 32 named routes"); }
  for (size_t at = 0; at < routes.size(); ++at) {
    const auto &route = routes[at];
    if (route.Id.empty() || route.Id.size() > kMaxNameBytes) {
      return std::unexpected("route id requires 1 to 64 bytes");
    }
    for (const char character : route.Id) {
      const bool letter =
          (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z');
      const bool digit = character >= '0' && character <= '9';
      if (!letter && !digit && character != '_' && character != '-' && character != '.') {
        return std::unexpected("route id accepts only ASCII letters, digits, '_', '-' and '.'");
      }
    }
    if (route.OsmRelationId == 0) {
      return std::unexpected("route osm relation id must be positive");
    }
    for (size_t earlier = 0; earlier < at; ++earlier) {
      if (routes[earlier].Id == route.Id) {
        return std::unexpected("duplicate route id '" + route.Id + "'");
      }
    }
  }
  return {};
}

}

#endif
