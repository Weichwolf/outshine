#ifndef OUTSHINE_CLIENTS_ASSEMBLY_H
#define OUTSHINE_CLIENTS_ASSEMBLY_H

#include <string>
#include <vector>

#include <outshine/Assembled.h>
#include <outshine/Scenario.h>

#include "Column.h"
#include "Store.h"

namespace outshine {

[[nodiscard]] size_t AssembledCapacity(const Scenario &declared);

[[nodiscard]] bool Assemble(const Scenario &declared, Store &into, Column<Vehicle> &vehicles,
                            Column<Drive> &driven, Assembled &out, std::string &error);

}

#endif
