#include "DeclaredSources.h"

#include <memory>

#include "StarBands.h"
#include "TerrariumDem.h"
#include "VersatilesVector.h"

namespace outshine::Data {

namespace {

constexpr const char *kKinds[] = {"terrain", "vector", "stars"};

[[nodiscard]] std::string Catalogue() {
  std::string all;
  for (const char *kind : kKinds) {
    if (!all.empty()) { all += ' '; }
    all += kind;
  }
  return all;
}

}

bool RegisterDeclared(SourceSet &set, std::span<const Provider> providers,
                      std::string_view starDirectory, std::string &error) {
  for (const Provider &provider : providers) {
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

std::span<const Provider> ShippedProviders() {
  static const Provider shipped[] = {
      {"terrain", "", 0, "hand over"},
      {"vector", "", 1, "hand over"},
      {"stars", "", 2, "hand over"},
  };
  return shipped;
}

}
