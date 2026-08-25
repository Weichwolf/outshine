#ifndef OUTSHINE_DATA_DECLAREDSOURCES_H
#define OUTSHINE_DATA_DECLAREDSOURCES_H

#include <span>
#include <string>

#include <Scenario.h>

#include "SourceSet.h"

namespace outshine::Data {

[[nodiscard]] bool RegisterDeclared(SourceSet &set, std::span<const Provider> providers,
                                    std::string_view starDirectory, std::string &error);

[[nodiscard]] std::span<const Provider> ShippedProviders();

}
#endif
