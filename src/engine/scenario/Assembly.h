#ifndef OUTSHINE_ENGINE_SCENARIO_ASSEMBLY_H
#define OUTSHINE_ENGINE_SCENARIO_ASSEMBLY_H

#include <expected>
#include <string>
#include <vector>

#include "Assembled.h"
#include "Traits.h"
#include <Scenario.h>

#include "Column.h"
#include <EntityRegistry.h>

namespace outshine {

[[nodiscard]] std::expected<size_t, std::string>
RequiredEntityCapacity(const Scenario::Document &declared);

[[nodiscard]] bool Assemble(const Scenario::Document &declared,
                            World::EntityRegistry &entities,
                            Column<Scenario::Body> &bodies,
                            Column<Traits> &traits,
                            Assembled &out,
                            std::string &error);

}

#endif
