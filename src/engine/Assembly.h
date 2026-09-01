#ifndef OUTSHINE_ENGINE_ASSEMBLY_H
#define OUTSHINE_ENGINE_ASSEMBLY_H

#include <string>
#include <vector>

#include "Assembled.h"
#include "Traits.h"
#include <scenario/Scenario.h>

#include "Column.h"
#include <scene/Scene.h>

namespace outshine {

[[nodiscard]] size_t AssembledCapacity(const Scenario::Document &declared);

[[nodiscard]] bool Assemble(const Scenario::Document &declared,
                            Scene &into,
                            Column<Scenario::Body> &bodies,
                            Column<Scenario::Journey> &driven,
                            Column<Traits> &traits,
                            Assembled &out,
                            std::string &error);

} // namespace outshine

#endif
