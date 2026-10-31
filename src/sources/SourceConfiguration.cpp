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

std::expected<std::vector<std::unique_ptr<Source>>, std::string>
PrepareSources(std::span<const SourceProvider> providers,
               std::string_view builtinDataDirectory,
               const ProviderRegistry &registry) {
  if (const auto valid = ValidateSourceProviders(providers, registry); !valid) {
    return std::unexpected(valid.error());
  }
  std::vector<std::unique_ptr<Source>> candidates;
  candidates.reserve(providers.size());
  for (const SourceProvider &provider : providers) {
    auto made = registry.named(provider.Kind)->make(provider, builtinDataDirectory);
    if (!made) { return std::unexpected(std::move(made.error())); }
    if (!*made) { return std::unexpected("provider factory returned a null source"); }
    candidates.push_back(std::move(*made));
  }
  const auto valid = SourceSet::ValidateSources(candidates);
  if (valid != SourceSet::Registration::Accepted) {
    return std::unexpected(std::string(SourceSet::RegistrationError(valid)));
  }
  return candidates;
}

bool RegisterSources(SourceSet &set,
                     std::vector<std::unique_ptr<Source>> sources,
                     std::string &error) {
  const auto registered = set.AddAll(std::move(sources));
  if (registered != SourceSet::Registration::Accepted) {
    error = SourceSet::RegistrationError(registered);
    return false;
  }
  return true;
}

bool RegisterSources(SourceSet &set,
                     std::span<const SourceProvider> providers,
                     std::string_view builtinDataDirectory,
                     std::string &error,
                     const ProviderRegistry &registry) {
  auto prepared = PrepareSources(providers, builtinDataDirectory, registry);
  if (!prepared) {
    error = std::move(prepared.error());
    return false;
  }
  return RegisterSources(set, std::move(*prepared), error);
}

}
