#include "SourceProviderValidation.h"

#include <cmath>
#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <utility>

namespace outshine::Data {

namespace {

constexpr double kLongitudeLimitDeg = 180.0;
constexpr double kLatitudeLimitDeg = 90.0;

[[nodiscard]] bool ValidCoverage(const SourceCoverage &bounds) noexcept {
  return std::isfinite(bounds.WestDeg) && std::isfinite(bounds.SouthDeg) &&
         std::isfinite(bounds.EastDeg) && std::isfinite(bounds.NorthDeg) &&
         bounds.WestDeg >= -kLongitudeLimitDeg && bounds.EastDeg <= kLongitudeLimitDeg &&
         bounds.SouthDeg >= -kLatitudeLimitDeg && bounds.NorthDeg <= kLatitudeLimitDeg &&
         bounds.WestDeg < bounds.EastDeg && bounds.SouthDeg < bounds.NorthDeg;
}

[[nodiscard]] bool DuplicateRank(std::span<const SourceProvider> providers, size_t at) {
  for (size_t previous = 0; previous < at; ++previous) {
    if (providers[previous].Kind == providers[at].Kind &&
        providers[previous].Priority == providers[at].Priority) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] std::expected<void, std::string> ValidateOsm(const SourceProvider &provider) {
  if (provider.Dataset.empty() || provider.Revision.empty() || provider.Location.empty()) {
    return std::unexpected("an osm provider requires dataset, pin and a local location");
  }
  if (provider.Missing != MissingDataPolicy::Fail) {
    return std::unexpected("an osm provider must fail when its source is absent");
  }
  const size_t colon = provider.Location.find(':');
  const size_t slash = provider.Location.find('/');
  if (colon != std::string::npos && (slash == std::string::npos || colon < slash)) {
    return std::unexpected("an osm provider location must be a local file path");
  }
  if (!provider.Coverage || !ValidCoverage(*provider.Coverage)) {
    return std::unexpected(
        "an osm provider requires finite west/south/east/north coverage without wrapping");
  }
  return {};
}

}

std::expected<void, std::string>
ValidateSourceProviders(std::span<const SourceProvider> providers) {
  const SourceProvider *firstOsm = nullptr;
  for (size_t at = 0; at < providers.size(); ++at) {
    const SourceProvider &provider = providers[at];
    if (provider.Kind.empty()) { return std::unexpected("a provider kind must not be empty"); }
    if (provider.Missing != MissingDataPolicy::Continue &&
        provider.Missing != MissingDataPolicy::Fail) {
      return std::unexpected("a provider declares an invalid missing-data policy");
    }
    if (DuplicateRank(providers, at)) {
      return std::unexpected("provider kind '" + provider.Kind + "' declares the same rank twice");
    }
    if (provider.Kind == "osm") {
      if (auto valid = ValidateOsm(provider); !valid) {
        return std::unexpected(std::move(valid.error()));
      }
      if (firstOsm != nullptr &&
          (firstOsm->Dataset != provider.Dataset || firstOsm->Revision != provider.Revision)) {
        return std::unexpected("osm chunks must share one dataset and revision");
      }
      if (firstOsm == nullptr) { firstOsm = &provider; }
    } else if (!provider.Dataset.empty() || !provider.Location.empty() || provider.Coverage) {
      return std::unexpected("only osm providers declare dataset, location or coverage");
    }
  }
  return {};
}

}
