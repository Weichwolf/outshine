#include "DeclaredSources.h"

#include <array>
#include <memory>
#include <string>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "StarBands.h"
#include "TerrariumDem.h"
#include "VersatilesVector.h"

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
      made = std::make_unique<TerrariumDem>(provider.Revision, order, provider.Missing);
    } else if (provider.Kind == "vector") {
      made = std::make_unique<VersatilesVector>(provider.Revision, order, provider.Missing);
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
  static const std::array<SourceProvider, 3> shipped = {{
      {.Kind = "terrain", .Revision = "", .Priority = 0, .Missing = MissingDataPolicy::Continue},
      {.Kind = "vector", .Revision = "", .Priority = 1, .Missing = MissingDataPolicy::Continue},
      {.Kind = "stars", .Revision = "", .Priority = 2, .Missing = MissingDataPolicy::Continue},
  }};
  return shipped;
}

}
