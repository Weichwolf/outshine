#ifndef OUTSHINE_WORLD_DATA_DECLAREDSOURCES_H
#define OUTSHINE_WORLD_DATA_DECLAREDSOURCES_H

#include <span>
#include <string>

#include <scenario/Scenario.h>

#include "SourceSet.h"

namespace outshine::Data {

[[nodiscard]] bool RegisterDeclared(SourceSet &set,
                                    std::span<const Scenario::Provider> providers,
                                    std::string_view starDirectory,
                                    std::string &error);

[[nodiscard]] std::span<const Scenario::Provider> ShippedProviders();

} // namespace outshine::Data
#endif
