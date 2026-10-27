#include "BuiltinSourceProviders.h"
#include "OsmProvider.h"
#include "CopernicusDemSource.h"
#include "TerrariumSource.h"
#include "VectorTileSource.h"
#include "StarCatalogueSource.h"
#include <array>
#include <initializer_list>
#include <cstddef>
#include <exception>
#include <expected>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace outshine::Generators {
namespace {
constexpr int kMapterhornMaximumZoom = 17;
constexpr size_t kTerrariumTypicalPayloadBytes = 60000;
constexpr size_t kTerrariumMaximumPayloadBytes = size_t{4} * 1024u * 1024u;

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

[[nodiscard]] std::expected<void, std::string> ValidateTile(const Data::SourceProvider &provider) {
  if (!provider.PayloadSha256.empty()) {
    return std::unexpected("this provider does not accept a response sha256");
  }

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
ValidateCopernicus(const Data::SourceProvider &provider) {
  if (!provider.PayloadSha256.empty()) {
    return std::unexpected("this provider does not accept a response sha256");
  }

  if (!provider.Location.empty() || provider.Coverage || !provider.PayloadSha256.empty()) {
    return std::unexpected(
        "Copernicus terrain does not accept a file, chunk coverage or whole-response digest");
  }
  if (!provider.Endpoint.empty() && provider.Endpoint != Terrain::CopernicusDemSource::Endpoint) {
    return std::unexpected(
        "the built-in terrain provider uses the official Copernicus GLO-30 endpoint");
  }
  return {};
}

[[nodiscard]] std::expected<void, std::string>
ValidateUnparameterized(const Data::SourceProvider &provider) {
  if (!provider.PayloadSha256.empty()) {
    return std::unexpected("this provider does not accept a response sha256");
  }

  if (!provider.Dataset.empty() || !provider.Location.empty() || !provider.Endpoint.empty() ||
      provider.Coverage) {
    return std::unexpected("this provider cannot declare dataset, location or endpoint");
  }
  return {};
}

class CopernicusProvider final : public Data::Provider {
public:
  [[nodiscard]] std::string_view kind() const override { return "copernicus"; }

  [[nodiscard]] std::expected<void, std::string>
  validate(const Data::SourceProvider &provider,
           [[maybe_unused]] std::span<const Data::SourceProvider> declared) const override {
    return ValidateCopernicus(provider);
  }

  [[nodiscard]] std::expected<std::unique_ptr<Data::Source>, std::string>
  make(const Data::SourceProvider &provider,
       [[maybe_unused]] std::string_view root) const override {
    return std::make_unique<Terrain::CopernicusDemSource>(
        provider.Revision,
        static_cast<Data::Rank>(provider.Priority),
        provider.Missing,
        provider.Dataset.empty() ? std::string(Terrain::CopernicusDemSource::Dataset)
                                 : provider.Dataset);
  }
};

class TerrariumProvider final : public Data::Provider {
public:
  [[nodiscard]] std::string_view kind() const override { return "terrain"; }

  [[nodiscard]] std::expected<void, std::string>
  validate(const Data::SourceProvider &provider,
           [[maybe_unused]] std::span<const Data::SourceProvider> declared) const override {
    return ValidateTile(provider);
  }

  [[nodiscard]] std::expected<std::unique_ptr<Data::Source>, std::string>
  make(const Data::SourceProvider &provider,
       [[maybe_unused]] std::string_view root) const override {
    return std::make_unique<Terrain::TerrariumSource>(Data::SourceDecl{
        .Id = provider.Dataset.empty() ? "mapterhorn.terrarium" : provider.Dataset,
        .Version = 1,
        .Revision = provider.Revision,
        .Endpoint = provider.Endpoint.empty() ? "https://tiles.mapterhorn.com/{z}/{x}/{y}.webp"
                                              : provider.Endpoint,
        .Kind = Data::DataKind::Elevation,
        .How = Data::Scheme::TileZxy,
        .Wire = Data::WireFormat::TerrariumWebp,
        .Order = static_cast<Data::Rank>(provider.Priority),
        .OnAbsent = provider.Missing,
        .MinZoom = 0,
        .MaxZoom = kMapterhornMaximumZoom,
        .AncestorFill = true,
        .Keeps = Data::Cacheability::Forever,
        .Need = Data::Necessity::Required,
        .Latency = Data::LatencyClass::Distant,
        .TypicalPayloadBytes = kTerrariumTypicalPayloadBytes,
        .RetryBudget = 4,
        .MaximumPayloadBytes = kTerrariumMaximumPayloadBytes,
        .PayloadSha256 = provider.PayloadSha256,
        .Schema = "",
        .TileAbsence = Data::TileAbsencePolicy::Parent});
  }
};

class VectorProvider final : public Data::Provider {
public:
  [[nodiscard]] std::string_view kind() const override { return "vector"; }

  [[nodiscard]] std::expected<void, std::string>
  validate(const Data::SourceProvider &provider,
           [[maybe_unused]] std::span<const Data::SourceProvider> declared) const override {
    if (auto valid = ValidateTile(provider); !valid) { return valid; }
    if (provider.Dataset.empty() || provider.Endpoint.empty()) {
      return std::unexpected(
          "a vector-tile fixture requires an explicit dataset and endpoint; no default exists");
    }
    if (!Osm::ParseMvtSchema(provider.Schema)) {
      return std::unexpected("unsupported vector schema: " + provider.Schema);
    }
    return {};
  }

  [[nodiscard]] std::expected<std::unique_ptr<Data::Source>, std::string>
  make(const Data::SourceProvider &provider,
       [[maybe_unused]] std::string_view root) const override {
    if (provider.Dataset.empty() || provider.Endpoint.empty()) {
      return std::unexpected(
          "a vector-tile fixture requires an explicit dataset and endpoint; no default exists");
    }
    const auto schema = Osm::ParseMvtSchema(provider.Schema);
    if (!schema) { return std::unexpected("unsupported vector schema: " + provider.Schema); }
    return std::make_unique<Osm::VectorTileSource>(provider.Revision,
                                                   static_cast<Data::Rank>(provider.Priority),
                                                   provider.Missing,
                                                   provider.Dataset,
                                                   provider.Endpoint,
                                                   *schema);
  }
};

class StarsProvider final : public Data::Provider {
public:
  [[nodiscard]] std::string_view kind() const override { return "stars"; }

  [[nodiscard]] std::expected<void, std::string>
  validate(const Data::SourceProvider &provider,
           [[maybe_unused]] std::span<const Data::SourceProvider> declared) const override {
    return ValidateUnparameterized(provider);
  }

  [[nodiscard]] std::expected<std::unique_ptr<Data::Source>, std::string>
  make(const Data::SourceProvider &provider, std::string_view root) const override {
    return std::make_unique<Sky::StarCatalogueSource>(
        (std::filesystem::path(root) / "sky").string(),
        provider.Revision,
        static_cast<Data::Rank>(provider.Priority),
        provider.Missing);
  }
};

}

void RegisterBuiltinSourceProviders(Data::ProviderRegistry &registry) {
  static const CopernicusProvider copernicus;
  static const TerrariumProvider terrain;
  static const VectorProvider vector;
  static const StarsProvider stars;
  static const Generators::Osm::Provider osm;
  const std::array<const Data::Provider *, 5> providers = {
      {&terrain, &copernicus, &vector, &stars, &osm}};
  for (const Data::Provider *provider : providers) {
    if (registry.named(provider->kind()) != nullptr) { continue; }
    const auto registered = registry.registerProvider(*provider);
    if (!registered &&
        registered.error() != Data::ProviderRegistry::RegistrationError::DuplicateKind) {
      std::terminate();
    }
  }
}

std::span<const Data::SourceProvider> DefaultSourceProviders() {
  static const std::array<Data::SourceProvider, 2> defaults = {{
      {.Kind = "terrain",
       .Revision = "",
       .Priority = 0,
       .Missing = Data::MissingDataPolicy::Fail,
       .Dataset = "",
       .Location = "",
       .Endpoint = "",
       .Coverage = {},
       .PayloadSha256 = "",
       .Schema = ""},
      {.Kind = "stars",
       .Revision = "",
       .Priority = 2,
       .Missing = Data::MissingDataPolicy::Continue,
       .Dataset = "",
       .Location = "",
       .Endpoint = "",
       .Coverage = {},
       .PayloadSha256 = "",
       .Schema = ""},
  }};
  return defaults;
}

}
