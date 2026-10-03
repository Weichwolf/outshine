#include "ShippedProviders.h"
#include "OsmProvider.h"
#include "CopernicusDem.h"
#include "TerrariumSource.h"
#include "VectorTileSource.h"
#include "StarBands.h"
#include <array>
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

class CopernicusProvider final : public Data::Provider {
public:
  [[nodiscard]] std::string_view kind() const override { return "copernicus"; }

  [[nodiscard]] std::expected<std::unique_ptr<Data::Source>, std::string>
  make(const Data::SourceProvider &provider,
       [[maybe_unused]] std::string_view root) const override {
    return std::make_unique<Data::CopernicusDem>(
        provider.Revision,
        static_cast<Data::Rank>(provider.Priority),
        provider.Missing,
        provider.Dataset.empty() ? std::string(Data::CopernicusDem::Dataset) : provider.Dataset);
  }
};

class TerrariumProvider final : public Data::Provider {
public:
  [[nodiscard]] std::string_view kind() const override { return "terrain"; }

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
        .TileAbsence = Data::TileAbsencePolicy::Parent});
  }
};

class VectorProvider final : public Data::Provider {
public:
  [[nodiscard]] std::string_view kind() const override { return "vector"; }

  [[nodiscard]] std::expected<std::unique_ptr<Data::Source>, std::string>
  make(const Data::SourceProvider &provider,
       [[maybe_unused]] std::string_view root) const override {
    if (provider.Dataset.empty() || provider.Endpoint.empty()) {
      return std::unexpected(
          "a vector-tile fixture requires an explicit dataset and endpoint; no default exists");
    }
    return std::make_unique<::outshine::Generators::Osm::VectorTileSource>(
        provider.Revision,
        static_cast<Data::Rank>(provider.Priority),
        provider.Missing,
        provider.Dataset,
        provider.Endpoint);
  }
};

class StarsProvider final : public Data::Provider {
public:
  [[nodiscard]] std::string_view kind() const override { return "stars"; }

  [[nodiscard]] std::expected<std::unique_ptr<Data::Source>, std::string>
  make(const Data::SourceProvider &provider, std::string_view root) const override {
    return std::make_unique<Data::StarBands>((std::filesystem::path(root) / "sky").string(),
                                             provider.Revision,
                                             static_cast<Data::Rank>(provider.Priority),
                                             provider.Missing);
  }
};

}

void RegisterShippedProviders(Data::ProviderRegistry &registry) {
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

std::span<const Data::SourceProvider> ShippedProviders() {
  static const std::array<Data::SourceProvider, 2> shipped = {{
      {.Kind = "terrain",
       .Revision = "",
       .Priority = 0,
       .Missing = Data::MissingDataPolicy::Fail,
       .Dataset = "",
       .Location = "",
       .Endpoint = "",
       .Coverage = {},
       .PayloadSha256 = ""},
      {.Kind = "stars",
       .Revision = "",
       .Priority = 2,
       .Missing = Data::MissingDataPolicy::Continue,
       .Dataset = "",
       .Location = "",
       .Endpoint = "",
       .Coverage = {},
       .PayloadSha256 = ""},
  }};
  return shipped;
}

}
