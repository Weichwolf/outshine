#ifndef OUTSHINE_ENGINE_ASSEMBLY_H
#define OUTSHINE_ENGINE_ASSEMBLY_H

#include <expected>
#include <string>
#include <vector>

#include "Assembled.h"
#include "Traits.h"
#include <scenario/Scenario.h>

#include "Column.h"
#include <scene/Scene.h>

namespace outshine {

[[nodiscard]] std::expected<size_t, std::string>
RequiredEntityCapacity(const Scenario::Document &declared);

[[nodiscard]] bool Assemble(const Scenario::Document &declared,
                            Scene &into,
                            Column<Scenario::Body> &bodies,
                            Column<Traits> &traits,
                            Assembled &out,
                            std::string &error);

}

#endif
