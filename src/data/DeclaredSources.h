#ifndef DECLAREDSOURCES_H
#define DECLAREDSOURCES_H

#include <span>
#include <string>

#include <outshine/Scenario.h>

#include "SourceSet.h"

namespace outshine::Data {

// the scenario declares which providers stand, each by its kind from THIS catalogue --
// the consumer selects and cannot add; a kind the engine does not carry refuses naming
// what it does carry
[[nodiscard]] bool RegisterDeclared(SourceSet &set, std::span<const Provider> providers,
                                    std::string_view starDirectory, std::string &error);

// the shipped battery: terrain, vector and stars at their catalogue ranks -- a CLIENT
// selects this convenience explicitly, the engine never assumes it
[[nodiscard]] std::span<const Provider> ShippedProviders();

}
#endif
