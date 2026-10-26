#ifndef OUTSHINE_SOURCES_SOURCEPROVIDERVALIDATION_H
#define OUTSHINE_SOURCES_SOURCEPROVIDERVALIDATION_H

#include <expected>
#include <span>
#include <string>

#include <SourceProvider.h>

namespace outshine::Data {
class ProviderRegistry;

[[nodiscard]] std::expected<void, std::string>
ValidateSourceProviders(std::span<const SourceProvider> providers);

[[nodiscard]] std::expected<void, std::string>
ValidateSourceProviders(std::span<const SourceProvider> providers,
                        const ProviderRegistry &registry);

}

#endif
