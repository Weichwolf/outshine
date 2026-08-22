#ifndef OUTSHINE_CLIENTS_ASSEMBLY_H
#define OUTSHINE_CLIENTS_ASSEMBLY_H

#include <string>
#include <vector>

#include <outshine/Scenario.h>

#include "Column.h"
#include "Store.h"

namespace outshine {

struct Assembled {
  std::vector<Entity> Bodies;
  Entity PlayerBody = kNoEntity;
  Entity PlayerMind = kNoEntity;
  Entity Nav = kNoEntity;
  Entity Assignment = kNoEntity;
};

[[nodiscard]] bool Assemble(const Scenario &declared, Store &into, Column<Vehicle> &vehicles,
                            Column<Drive> &driven, Assembled &out, std::string &error);

}

#endif
