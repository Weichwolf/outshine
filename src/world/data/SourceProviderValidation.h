#ifndef OUTSHINE_WORLD_DATA_SOURCEPROVIDERVALIDATION_H
#define OUTSHINE_WORLD_DATA_SOURCEPROVIDERVALIDATION_H

#include <expected>
#include <span>
#include <string>

#include <world/SourceProvider.h>

namespace outshine::Data {

[[nodiscard]] std::expected<void, std::string>
ValidateSourceProviders(std::span<const SourceProvider> providers);

}

#endif
