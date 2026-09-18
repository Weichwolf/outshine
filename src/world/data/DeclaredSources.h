#ifndef OUTSHINE_WORLD_DATA_DECLAREDSOURCES_H
#define OUTSHINE_WORLD_DATA_DECLAREDSOURCES_H

#include <span>
#include <string>

#include <world/SourceProvider.h>

#include "SourceSet.h"

namespace outshine::Data {

[[nodiscard]] bool RegisterDeclared(SourceSet &set,
                                    std::span<const SourceProvider> providers,
                                    std::string_view starDirectory,
                                    std::string &error);

[[nodiscard]] std::span<const SourceProvider> ShippedProviders();

}
#endif
