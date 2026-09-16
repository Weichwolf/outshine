#include "DeclaredSources.h"

#include <array>
#include <memory>
#include <optional>
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

[[nodiscard]] std::optional<AbsencePolicy> ParseAbsence(std::string_view said) {
  if (said.empty() || said == "hand over") { return AbsencePolicy::HandOver; }
  if (said == "fail") { return AbsencePolicy::Refuse; }
  return std::nullopt;
}

}

bool RegisterDeclared(SourceSet &set,
                      std::span<const Scenario::Provider> providers,
                      std::string_view starDirectory,
                      std::string &error) {
  for (const Scenario::Provider &provider : providers) {
    const std::optional<AbsencePolicy> absence = ParseAbsence(provider.WhenAbsent);
    if (!absence) {
      error = "the provider of kind '" + provider.Kind + "' declares unknown whenAbsent policy '" +
              provider.WhenAbsent + "'; this engine carries: hand over fail";
      return false;
    }
    const Rank order = static_cast<Rank>(provider.Rank);
    std::unique_ptr<Source> made;
    if (provider.Kind == "terrain") {
      made = std::make_unique<TerrariumDem>(provider.Pin, order, *absence);
    } else if (provider.Kind == "vector") {
      made = std::make_unique<VersatilesVector>(provider.Pin, order, *absence);
    } else if (provider.Kind == "stars") {
      made = std::make_unique<StarBands>(std::string(starDirectory), provider.Pin, order, *absence);
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

}
