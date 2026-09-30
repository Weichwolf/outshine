#include "SourceProviderValidation.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <expected>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace outshine::Data {

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

[[nodiscard]] bool ValidEndpoint(std::string_view endpoint) {
  if (!endpoint.starts_with("https://") || endpoint.size() <= 8 ||
      endpoint.find_first_of(" \t\r\n") != std::string_view::npos) {
    return false;
  }
  for (const std::string_view token : {"{z}", "{x}", "{y}"}) {
    const size_t at = endpoint.find(token);
    if (at == std::string_view::npos ||
        endpoint.find(token, at + token.size()) != std::string_view::npos) {
      return false;
    }
  }
  std::string remaining(endpoint);
  for (const std::string_view token : {"{z}", "{x}", "{y}"}) {
    remaining.erase(remaining.find(token), token.size());
  }
  return remaining.find_first_of("{}") == std::string::npos;
}

[[nodiscard]] std::expected<void, std::string> ValidateOsmLocation(const SourceProvider &provider,
                                                                   const SourceCoverage &bounds) {
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

[[nodiscard]] std::expected<void, std::string> ValidateOsmPins(const SourceProvider &provider) {
  if (provider.Dataset.empty() || provider.Revision.empty()) {
    return std::unexpected("an osm provider requires dataset and revision");
  }
  if (provider.Missing != MissingDataPolicy::Fail) {
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

[[nodiscard]] std::expected<void, std::string> ValidateOsm(const SourceProvider &provider) {
  if (auto valid = ValidateOsmPins(provider); !valid) { return valid; }
  if (!provider.Coverage || !ValidCoverage(*provider.Coverage)) {
    return std::unexpected(
        "an osm provider requires finite west/south/east/north coverage without wrapping");
  }
  return ValidateOsmLocation(provider, *provider.Coverage);
}

[[nodiscard]] std::expected<void, std::string> ValidateTile(const SourceProvider &provider) {
  if (!provider.Location.empty() || provider.Coverage) {
    return std::unexpected("tile providers cannot declare location or coverage");
  }
  if (provider.Endpoint.empty() != provider.Dataset.empty()) {
    return std::unexpected("a tile endpoint requires a stable dataset and vice versa");
  }
  if (!provider.Endpoint.empty() && !ValidEndpoint(provider.Endpoint)) {
    return std::unexpected("a tile endpoint requires HTTPS and one each of {z}, {x}, {y}");
  }
  return {};
}

[[nodiscard]] std::expected<void, std::string>
ValidateUnparameterized(const SourceProvider &provider) {
  if (!provider.Dataset.empty() || !provider.Location.empty() || !provider.Endpoint.empty() ||
      provider.Coverage) {
    return std::unexpected("this provider cannot declare dataset, location or endpoint");
  }
  return {};
}

[[nodiscard]] std::expected<void, std::string> ValidateSource(const SourceProvider &provider,
                                                              const SourceProvider *&firstOsm) {
  if (provider.Kind == "osm") {
    if (auto valid = ValidateOsm(provider); !valid) {
      return std::unexpected(std::move(valid.error()));
    }
    if (firstOsm != nullptr &&
        (firstOsm->Dataset != provider.Dataset || firstOsm->Revision != provider.Revision)) {
      return std::unexpected("osm chunks must share one dataset and revision");
    }
    if (firstOsm == nullptr) { firstOsm = &provider; }
    return {};
  }
  if (provider.Kind == "terrain" || provider.Kind == "vector") { return ValidateTile(provider); }
  if (provider.Kind == "stars") { return ValidateUnparameterized(provider); }
  return {};
}

}

std::expected<void, std::string>
ValidateSourceProviders(std::span<const SourceProvider> providers) {
  const SourceProvider *firstOsm = nullptr;
  for (size_t at = 0; at < providers.size(); ++at) {
    const SourceProvider &provider = providers[at];
    if (provider.Kind.empty()) { return std::unexpected("a provider kind must not be empty"); }
    if (provider.Kind != "osm" && !provider.PayloadSha256.empty()) {
      return std::unexpected("only an osm provider can declare a response sha256");
    }
    if (provider.Missing != MissingDataPolicy::Continue &&
        provider.Missing != MissingDataPolicy::Fail) {
      return std::unexpected("a provider declares an invalid missing-data policy");
    }
    if (DuplicateRank(providers, at)) {
      return std::unexpected("provider kind '" + provider.Kind + "' declares the same rank twice");
    }
    if (auto valid = ValidateSource(provider, firstOsm); !valid) {
      return std::unexpected(std::move(valid.error()));
    }
  }
  return {};
}

}
