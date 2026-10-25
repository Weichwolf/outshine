#include "SourceProviderValidation.h"
#include "Provider.h"
#include <cstddef>
#include <expected>
#include <span>
#include <string>

namespace outshine::Data {
namespace {
[[nodiscard]] bool DuplicateRank(std::span<const SourceProvider> providers, size_t at) {
  for (size_t previous = 0; previous < at; ++previous) {
    if (providers[previous].Kind == providers[at].Kind &&
        providers[previous].Priority == providers[at].Priority) {
      return true;
    }
  }
  return false;
}
}

std::expected<void, std::string>
ValidateSourceProviders(std::span<const SourceProvider> providers) {
  for (size_t at = 0; at < providers.size(); ++at) {
    const SourceProvider &provider = providers[at];
    if (provider.Kind.empty()) { return std::unexpected("a provider kind must not be empty"); }
    if (provider.Missing != MissingDataPolicy::Continue &&
        provider.Missing != MissingDataPolicy::Fail) {
      return std::unexpected("a provider declares an invalid missing-data policy");
    }
    if (DuplicateRank(providers, at)) {
      return std::unexpected("provider kind '" + provider.Kind + "' declares the same rank twice");
    }
  }
  return {};
}

std::expected<void, std::string> ValidateSourceProviders(std::span<const SourceProvider> providers,
                                                         const ProviderRegistry &registry) {
  if (auto valid = ValidateSourceProviders(providers); !valid) { return valid; }
  for (const auto &provider : providers) {
    const Provider *factory = registry.named(provider.Kind);
    if (factory == nullptr) {
      return std::unexpected("the scenario declares an unavailable provider of kind '" +
                             provider.Kind + "'");
    }
    if (auto valid = factory->validate(provider, providers); !valid) { return valid; }
  }
  return {};
}

}
