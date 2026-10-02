#ifndef OUTSHINE_GENERATORS_BUILDING_STRUCTURESOURCEINPUTS_H
#define OUTSHINE_GENERATORS_BUILDING_STRUCTURESOURCEINPUTS_H

#include "SourceObjects.h"
#include "SourceProvenance.h"
#include <memory>

namespace outshine::Generators {

struct StructureSourceInputs {
  std::shared_ptr<const Data::SourceObjects> Objects;
  std::weak_ptr<const void> Archive;
  Data::ProductOrigin Origin{};
};

}

#endif
