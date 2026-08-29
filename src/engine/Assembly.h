#ifndef OUTSHINE_ENGINE_ASSEMBLY_H
#define OUTSHINE_ENGINE_ASSEMBLY_H

#include <string>
#include <vector>

#include "Assembled.h"
#include "Traits.h"
#include <Scenario.h>

#include "Column.h"
#include <Scene.h>

namespace outshine {

[[nodiscard]] size_t AssembledCapacity(const Scenario &declared);

[[nodiscard]] bool Assemble(const Scenario &declared, Scene &into, Column<Body> &bodies,
                            Column<Journey> &driven, Column<Traits> &traits, Assembled &out,
                            std::string &error);

}

#endif
