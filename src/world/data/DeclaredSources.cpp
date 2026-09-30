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
#include "TerrariumDem.h"
#include "VectorTileSource.h"

namespace outshine::Data {

namespace {

constexpr std::array<const char *, 3> kKinds = {{"terrain", "vector", "stars"}};

[[nodiscard]] std::string Catalogue() {
  std::string all;
  for (const char *kind : kKinds) {
    if (!all.empty()) { all += ' '; }
    all += kind;
  }
  return all;
}

}

bool RegisterDeclared(SourceSet &set,
                      std::span<const SourceProvider> providers,
                      std::string_view starDirectory,
                      std::string &error) {
  if (const auto valid = ValidateSourceProviders(providers); !valid) {
    error = valid.error();
    return false;
  }
  std::vector<std::unique_ptr<Source>> candidates;
  candidates.reserve(providers.size());
  for (const SourceProvider &provider : providers) {
    if (provider.Missing != MissingDataPolicy::Continue &&
        provider.Missing != MissingDataPolicy::Fail) {
      error = "the provider of kind '" + provider.Kind + "' declares an invalid missing policy";
      return false;
    }
    const Rank order = static_cast<Rank>(provider.Priority);
    std::unique_ptr<Source> made;
    if (provider.Kind == "terrain") {
      made = std::make_unique<TerrariumDem>(
          provider.Revision, order, provider.Missing, provider.Dataset, provider.Endpoint);
    } else if (provider.Kind == "vector") {
      if (provider.Dataset.empty() || provider.Endpoint.empty()) {
        error =
            "a vector-tile fixture requires an explicit dataset and endpoint; no default exists";
        return false;
      }
      made = std::make_unique<VectorTileSource>(
          provider.Revision, order, provider.Missing, provider.Dataset, provider.Endpoint);
    } else if (provider.Kind == "stars") {
      made = std::make_unique<StarBands>(
          std::string(starDirectory), provider.Revision, order, provider.Missing);
    } else {
      error = "the scenario declares a provider of kind '" + provider.Kind +
              "', and this engine carries: " + Catalogue();
      return false;
    }
    candidates.push_back(std::move(made));
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
       .Missing = MissingDataPolicy::Continue,
       .Dataset = "",
       .Location = "",
       .Endpoint = "",
       .Coverage = {}},
      {.Kind = "stars",
       .Revision = "",
       .Priority = 2,
       .Missing = MissingDataPolicy::Continue,
       .Dataset = "",
       .Location = "",
       .Endpoint = "",
       .Coverage = {}},
  }};
  return shipped;
}

}
