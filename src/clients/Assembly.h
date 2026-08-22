#ifndef OUTSHINE_CLIENTS_ASSEMBLY_H
#define OUTSHINE_CLIENTS_ASSEMBLY_H

#include <string>
#include <vector>

#include <outshine/Scenario.h>

#include "Store.h"

namespace outshine {

struct Assembled {
  std::vector<Entity> Bodies;
  Entity PlayerBody = kNoEntity;
  Entity PlayerMind = kNoEntity;
};

[[nodiscard]] bool Assemble(const Scenario &declared, Store &into, Assembled &out,
                            std::string &error);

}

#endif
