#ifndef OUTSHINE_GENERATORS_SHIPPEDPROVIDERS_H
#define OUTSHINE_GENERATORS_SHIPPEDPROVIDERS_H
#include <Provider.h>
#include <SourceProvider.h>
#include <span>

namespace outshine::Generators {
void RegisterShippedProviders(Data::ProviderRegistry &registry);
[[nodiscard]] std::span<const Data::SourceProvider> ShippedProviders();
}
#endif
