#include "SourceConfiguration.h"
#include "SourceProviderValidation.h"
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Data {
std::expected<std::unique_ptr<Source>, std::string>
ConfigureSource(const SourceProvider &provider,
                std::string_view builtinDataDirectory,
                const ProviderRegistry &registry) {
  if (auto valid = ValidateSourceProviders(std::span(&provider, 1), registry); !valid) {
    return std::unexpected(std::move(valid.error()));
  }
  const Provider *factory = registry.named(provider.Kind);
  if (factory == nullptr) {
    return std::unexpected("the scenario declares an unavailable provider of kind '" +
                           provider.Kind + "'");
  }
  auto source = factory->make(provider, builtinDataDirectory);
  if (source && !*source) { return std::unexpected("provider factory returned a null source"); }
  return source;
}

bool RegisterSources(SourceSet &set,
                     std::span<const SourceProvider> providers,
                     std::string_view builtinDataDirectory,
                     std::string &error,
                     const ProviderRegistry &registry) {
  if (const auto valid = ValidateSourceProviders(providers, registry); !valid) {
    error = valid.error();
    return false;
  }
  std::vector<std::unique_ptr<Source>> candidates;
  candidates.reserve(providers.size());
  for (const SourceProvider &provider : providers) {
    auto made = registry.named(provider.Kind)->make(provider, builtinDataDirectory);
    if (!made) {
      error = std::move(made.error());
      return false;
    }
    if (!*made) {
      error = "provider factory returned a null source";
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
    case SourceSet::Registration::InvalidDefinition:
      error = "a provider resolves to source metadata without a valid persistent identity";
      return false;
  }
  return true;
}

}
