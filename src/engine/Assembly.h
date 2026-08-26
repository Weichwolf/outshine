#ifndef OUTSHINE_ENGINE_ASSEMBLY_H
#define OUTSHINE_ENGINE_ASSEMBLY_H

#include <string>
#include <vector>

#include "Assembled.h"
#include "Traits.h"
#include <Scenario.h>

#include "Column.h"
#include "Store.h"

namespace outshine {

[[nodiscard]] size_t AssembledCapacity(const Scenario &declared);

[[nodiscard]] bool Assemble(const Scenario &declared, Store &into, Column<Body> &vehicles,
                            Column<Drive> &driven, Column<Traits> &traits, Assembled &out,
                            std::string &error);

}

#endif
