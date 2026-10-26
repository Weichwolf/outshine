#include "OsmProviderValidation.h"
#include <SourceProviderValidation.h>
#include <GeographicBounds.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <expected>
#include <ranges>
#include <span>
#include <string>
#include <string_view>

namespace outshine::Generators::Osm {
namespace {
constexpr double kLongitudeLimitDeg = 180.0;
constexpr double kLatitudeLimitDeg = 90.0;
constexpr std::string_view kSha256PinPrefix = "sha256:";
constexpr size_t kSha256HexDigits = 64;

[[nodiscard]] bool ValidDigest(std::string_view digest) {
  return digest.size() == kSha256HexDigits && std::ranges::all_of(digest, [](char digit) {
           return (digit >= '0' && digit <= '9') || (digit >= 'a' && digit <= 'f');
         });
}

[[nodiscard]] bool ValidCoverage(const GeographicBounds &bounds) noexcept {
  return std::isfinite(bounds.WestDeg) && std::isfinite(bounds.SouthDeg) &&
         std::isfinite(bounds.EastDeg) && std::isfinite(bounds.NorthDeg) &&
         bounds.WestDeg >= -kLongitudeLimitDeg && bounds.EastDeg <= kLongitudeLimitDeg &&
         bounds.SouthDeg >= -kLatitudeLimitDeg && bounds.NorthDeg <= kLatitudeLimitDeg &&
         bounds.WestDeg < bounds.EastDeg && bounds.SouthDeg < bounds.NorthDeg;
}

[[nodiscard]] std::expected<void, std::string>
ValidateOsmLocation(const Data::SourceProvider &provider, const GeographicBounds &bounds) {
  if (provider.Location.empty() == provider.Endpoint.empty()) {
    return std::unexpected(
        "an osm provider requires exactly one local location or official API endpoint");
  }
  if (!provider.Endpoint.empty()) {
    if (provider.Endpoint != kOfficialOsmApi) {
      return std::unexpected("an osm endpoint must be the official original OSM API");
    }
    if ((bounds.EastDeg - bounds.WestDeg) * (bounds.NorthDeg - bounds.SouthDeg) >
        kOsmApiMaximumAreaDeg2) {
      return std::unexpected(
          "an original OSM API map request exceeds its 0.25 square-degree area limit");
    }
    return {};
  }
  const size_t colon = provider.Location.find(':');
  const size_t slash = provider.Location.find('/');
  if (colon != std::string::npos && (slash == std::string::npos || colon < slash)) {
    return std::unexpected("an osm provider location must be a local file path");
  }
  return {};
}

[[nodiscard]] std::expected<void, std::string>
ValidateOsmPins(const Data::SourceProvider &provider) {
  if (provider.Dataset.empty() || provider.Revision.empty()) {
    return std::unexpected("an osm provider requires dataset and revision");
  }
  if (provider.Missing != Data::MissingDataPolicy::Fail) {
    return std::unexpected("an osm provider must fail when its source is absent");
  }
  if (std::string_view(provider.Revision).starts_with(kSha256PinPrefix)) {
    const std::string_view digest =
        std::string_view(provider.Revision).substr(kSha256PinPrefix.size());
    if (!ValidDigest(digest)) {
      return std::unexpected("an osm sha256 pin requires 64 lowercase hexadecimal digits");
    }
    if (!provider.PayloadSha256.empty() && provider.PayloadSha256 != digest) {
      return std::unexpected("an osm response digest contradicts its legacy sha256 pin");
    }
  }
  if (!provider.PayloadSha256.empty() && !ValidDigest(provider.PayloadSha256)) {
    return std::unexpected("an osm response sha256 requires 64 lowercase hexadecimal digits");
  }
  return {};
}

[[nodiscard]] std::expected<void, std::string> ValidateOsm(const Data::SourceProvider &provider) {
  if (auto valid = ValidateOsmPins(provider); !valid) { return valid; }
  if (!provider.Coverage && provider.Location.empty() && provider.Endpoint == kOfficialOsmApi) {
    if (!provider.PayloadSha256.empty() ||
        std::string_view(provider.Revision).starts_with(kSha256PinPrefix)) {
      return std::unexpected("an original OSM catalogue requires independent cell payload pins");
    }
    return {};
  }
  if (!provider.Coverage || !ValidCoverage(*provider.Coverage)) {
    return std::unexpected(
        "an osm provider requires finite west/south/east/north coverage without wrapping");
  }
  return ValidateOsmLocation(provider, *provider.Coverage);
}

}

std::expected<void, std::string> ValidateProvider(const Data::SourceProvider &provider,
                                                  std::span<const Data::SourceProvider> declared) {
  if (provider.Kind != "osm") {
    return std::unexpected("original OSM validation requires an osm provider");
  }
  if (auto valid = ValidateOsm(provider); !valid) { return valid; }
  for (const auto &other : declared) {
    if (other.Kind == "osm" &&
        (other.Dataset != provider.Dataset || other.Revision != provider.Revision)) {
      return std::unexpected("osm chunks must share one dataset and revision");
    }
  }
  return {};
}

std::expected<void, std::string>
ValidateProviders(std::span<const Data::SourceProvider> providers) {
  if (auto valid = Data::ValidateSourceProviders(providers); !valid) { return valid; }
  for (const auto &provider : providers) {
    if (auto valid = ValidateProvider(provider, providers); !valid) { return valid; }
  }
  return {};
}
}
