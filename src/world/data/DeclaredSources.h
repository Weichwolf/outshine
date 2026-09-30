#ifndef OUTSHINE_WORLD_DATA_DECLAREDSOURCES_H
#define OUTSHINE_WORLD_DATA_DECLAREDSOURCES_H

#include <span>
#include <string>

#include <world/SourceProvider.h>
#include <world/Provider.h>

#include "SourceSet.h"

namespace outshine::Data {

[[nodiscard]] bool RegisterDeclared(SourceSet &set,
                                    std::span<const SourceProvider> providers,
                                    std::string_view starDirectory,
                                    std::string &error,
                                    const ProviderRegistry *registry = nullptr);

void RegisterShippedProviders(ProviderRegistry &registry);

[[nodiscard]] std::expected<std::unique_ptr<Source>, std::string>
MakeDeclaredSource(const SourceProvider &provider,
                   std::string_view shippedRoot,
                   const ProviderRegistry *registry = nullptr);

[[nodiscard]] std::span<const SourceProvider> ShippedProviders();

}
#endif
