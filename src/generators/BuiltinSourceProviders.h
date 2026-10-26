#ifndef OUTSHINE_GENERATORS_BUILTINSOURCEPROVIDERS_H
#define OUTSHINE_GENERATORS_BUILTINSOURCEPROVIDERS_H
#include <Provider.h>
#include <SourceProvider.h>
#include <span>

namespace outshine::Generators {
void RegisterBuiltinSourceProviders(Data::ProviderRegistry &registry);
[[nodiscard]] std::span<const Data::SourceProvider> DefaultSourceProviders();
}
#endif
