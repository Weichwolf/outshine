#include "DeclaredSources.h"

#include <array>
#include <memory>
#include <string>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "StarBands.h"
#include "SourceProviderValidation.h"
#include "CopernicusDem.h"
#include "VectorTileSource.h"
#include "OsmApiSource.h"
#include "ReadTextFile.h"
#include "OsmXmlReader.h"
#include <expected>
#include <exception>
#include <filesystem>
#include <cstdint>

namespace outshine::Data {

namespace {

class LocalOsmSource final : public Source {
public:
  LocalOsmSource(const SourceProvider &provider, std::string_view root) {
    const std::filesystem::path location(provider.Location);
    Decl_.Id = provider.Dataset;
    Decl_.Revision = provider.Revision;
    Decl_.Endpoint =
        (location.is_absolute() ? location : std::filesystem::path(root) / location).string();
    Decl_.Kind = DataKind::OriginalOsm;
    Decl_.How = Scheme::WholeWorld;
    Decl_.Wire = WireFormat::OsmXml;
    Decl_.Order = static_cast<Rank>(provider.Priority);
    Decl_.OnAbsent = AbsencePolicy::Fail;
    Decl_.Keeps = Cacheability::Never;
    Decl_.Latency = LatencyClass::Local;
    Decl_.MaximumPayloadBytes = kMaxOsmXmlBytes;
    Decl_.PayloadSha256 = provider.PayloadSha256;
  }

  [[nodiscard]] const SourceDecl &Declaration() const noexcept override { return Decl_; }

  [[nodiscard]] Coverage Covers(const Fetch &request) const noexcept override {
    return request.Kind() == DataKind::OriginalOsm && request.Where().Index() == 0
               ? Coverage::Inside
               : Coverage::Outside;
  }

  [[nodiscard]] Address Serves(const Fetch &request) const noexcept override {
    return request.Where();
  }

  FetchStart Begin(const Address &at, [[maybe_unused]] Transport &transport) const override {
    return at.Index() == 0 ? FetchStart(Ticket::None)
                           : std::unexpected(FetchFailureReason::InvalidRequest);
  }

  Fetched Collect(const Address &at,
                  [[maybe_unused]] Ticket ticket,
                  [[maybe_unused]] Transport &transport) const override {
    if (at.Index() != 0) {
      return Fetched::Meant(Meaning::Refused, FetchFailureReason::InvalidRequest);
    }
    auto text = ReadTextFile(Decl_.Endpoint, kMaxOsmXmlBytes);
    if (!text) { return Fetched::Meant(Meaning::Refused); }
    return Fetched::Delivered(std::vector<uint8_t>(text->begin(), text->end()));
  }

private:
  SourceDecl Decl_;
};

class TerrainProvider final : public Provider {
public:
  [[nodiscard]] std::string_view kind() const override { return "terrain"; }

  [[nodiscard]] std::expected<std::unique_ptr<Source>, std::string>
  make(const SourceProvider &provider, [[maybe_unused]] std::string_view root) const override {
    return std::make_unique<CopernicusDem>(
        provider.Revision,
        static_cast<Rank>(provider.Priority),
        provider.Missing,
        provider.Dataset.empty() ? std::string(CopernicusDem::Dataset) : provider.Dataset);
  }
};

class VectorProvider final : public Provider {
public:
  [[nodiscard]] std::string_view kind() const override { return "vector"; }

  [[nodiscard]] std::expected<std::unique_ptr<Source>, std::string>
  make(const SourceProvider &provider, [[maybe_unused]] std::string_view root) const override {
    if (provider.Dataset.empty() || provider.Endpoint.empty()) {
      return std::unexpected(
          "a vector-tile fixture requires an explicit dataset and endpoint; no default exists");
    }
    return std::make_unique<VectorTileSource>(provider.Revision,
                                              static_cast<Rank>(provider.Priority),
                                              provider.Missing,
                                              provider.Dataset,
                                              provider.Endpoint);
  }
};

class StarsProvider final : public Provider {
public:
  [[nodiscard]] std::string_view kind() const override { return "stars"; }

  [[nodiscard]] std::expected<std::unique_ptr<Source>, std::string>
  make(const SourceProvider &provider, std::string_view root) const override {
    return std::make_unique<StarBands>((std::filesystem::path(root) / "sky").string(),
                                       provider.Revision,
                                       static_cast<Rank>(provider.Priority),
                                       provider.Missing);
  }
};

class OriginalOsmProvider final : public Provider {
public:
  [[nodiscard]] std::string_view kind() const override { return "osm"; }

  [[nodiscard]] std::expected<std::unique_ptr<Source>, std::string>
  make(const SourceProvider &provider, std::string_view root) const override {
    if (provider.Endpoint.empty()) { return std::make_unique<LocalOsmSource>(provider, root); }
    auto made = OsmApiSource::Create(provider, 0);
    if (!made) { return std::unexpected(std::move(made.error())); }
    return std::move(*made);
  }
};

}

void RegisterShippedProviders(ProviderRegistry &registry) {
  static const TerrainProvider terrain;
  static const VectorProvider vector;
  static const StarsProvider stars;
  static const OriginalOsmProvider osm;
  const std::array<const Provider *, 4> providers = {{&terrain, &vector, &stars, &osm}};
  for (const Provider *provider : providers) {
    if (registry.named(provider->kind()) != nullptr) { continue; }
    const auto registered = registry.registerProvider(*provider);
    if (!registered && registered.error() != ProviderRegistry::RegistrationError::DuplicateKind) {
      std::terminate();
    }
  }
}

std::expected<std::unique_ptr<Source>, std::string> MakeDeclaredSource(
    const SourceProvider &provider, std::string_view root, const ProviderRegistry *registry) {
  if (auto valid = ValidateSourceProviders(std::span(&provider, 1)); !valid) {
    return std::unexpected(std::move(valid.error()));
  }
  if (registry == nullptr) {
    ProviderRegistry shipped;
    RegisterShippedProviders(shipped);
    return MakeDeclaredSource(provider, root, &shipped);
  }
  const Provider *factory = registry->named(provider.Kind);
  if (factory == nullptr) {
    return std::unexpected("the scenario declares an unavailable provider of kind '" +
                           provider.Kind + "'");
  }
  auto source = factory->make(provider, root);
  if (source && !*source) { return std::unexpected("provider factory returned a null source"); }
  return source;
}

bool RegisterDeclared(SourceSet &set,
                      std::span<const SourceProvider> providers,
                      std::string_view starDirectory,
                      std::string &error,
                      const ProviderRegistry *registry) {
  if (const auto valid = ValidateSourceProviders(providers); !valid) {
    error = valid.error();
    return false;
  }
  std::vector<std::unique_ptr<Source>> candidates;
  candidates.reserve(providers.size());
  for (const SourceProvider &provider : providers) {
    const auto root = std::filesystem::path(starDirectory).parent_path().string();
    auto made = MakeDeclaredSource(provider, root, registry);
    if (!made) {
      error = std::move(made.error());
      return false;
    }
    candidates.push_back(std::move(*made));
  }
  switch (set.AddAll(std::move(candidates))) {
    case SourceSet::Registration::Accepted: break;
    case SourceSet::Registration::DuplicateRank:
      error = "the providers declare one source kind and priority twice";
      return false;
    case SourceSet::Registration::Unnamed:
      error = "a provider resolves to a source without an id";
      return false;
    case SourceSet::Registration::Sealed:
      error = "providers cannot be registered after the tile pool starts";
      return false;
  }
  return true;
}

std::span<const SourceProvider> ShippedProviders() {
  static const std::array<SourceProvider, 2> shipped = {{
      {.Kind = "terrain",
       .Revision = "",
       .Priority = 0,
       .Missing = MissingDataPolicy::Fail,
       .Dataset = "",
       .Location = "",
       .Endpoint = "",
       .Coverage = {},
       .PayloadSha256 = ""},
      {.Kind = "stars",
       .Revision = "",
       .Priority = 2,
       .Missing = MissingDataPolicy::Continue,
       .Dataset = "",
       .Location = "",
       .Endpoint = "",
       .Coverage = {},
       .PayloadSha256 = ""},
  }};
  return shipped;
}

}
