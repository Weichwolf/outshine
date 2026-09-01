#include "DeclaredSources.h"

#include <array>
#include <memory>
#include <string>
#include <span>
#include <string_view>
#include <utility>

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

} // namespace

bool RegisterDeclared(SourceSet &set,
                      std::span<const Scenario::Provider> providers,
                      std::string_view starDirectory,
                      std::string &error) {
  for (const Scenario::Provider &provider : providers) {
    std::unique_ptr<Source> made;
    if (provider.Kind == "terrain") {
      made = std::make_unique<TerrariumDem>();
    } else if (provider.Kind == "vector") {
      made = std::make_unique<VersatilesVector>();
    } else if (provider.Kind == "stars") {
      made = std::make_unique<StarBands>(std::string(starDirectory));
    } else {
      error = "the scenario declares a provider of kind '" + provider.Kind +
              "', and this engine carries: " + Catalogue();
      return false;
    }
    switch (set.Add(std::move(made))) {
      case SourceSet::Registration::Accepted: break;
      case SourceSet::Registration::DuplicateRank:
        error = "the scenario declares two providers of kind '" + provider.Kind +
                "' at one rank, and a lookup with two answers has none";
        return false;
      case SourceSet::Registration::Unnamed:
        error = "the provider of kind '" + provider.Kind + "' carries no id";
        return false;
    }
  }
  return true;
}

std::span<const Scenario::Provider> ShippedProviders() {
  static const std::array<Scenario::Provider, 3> shipped = {{
      {.Kind = "terrain", .Pin = "", .Rank = 0, .WhenAbsent = "hand over"},
      {.Kind = "vector", .Pin = "", .Rank = 1, .WhenAbsent = "hand over"},
      {.Kind = "stars", .Pin = "", .Rank = 2, .WhenAbsent = "hand over"},
  }};
  return shipped;
}

} // namespace outshine::Data
