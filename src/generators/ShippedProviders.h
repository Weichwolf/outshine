#ifndef OUTSHINE_GENERATORS_SHIPPEDPROVIDERS_H
#define OUTSHINE_GENERATORS_SHIPPEDPROVIDERS_H
#include <world/Provider.h>
#include <world/SourceProvider.h>
#include <span>

namespace outshine::Generators {
void RegisterShippedProviders(Data::ProviderRegistry &registry);
[[nodiscard]] std::span<const Data::SourceProvider> ShippedProviders();
}
#endif
