#ifndef OUTSHINE_GENERATORS_BUILDING_ORIGINALSTRUCTURESOURCE_H
#define OUTSHINE_GENERATORS_BUILDING_ORIGINALSTRUCTURESOURCE_H

#include "OsmSourceSnapshot.h"
#include "SourceProvenance.h"
#include <memory>

namespace outshine::Generators {

struct OriginalStructureSource {
  std::shared_ptr<const Data::OsmSourceSnapshot> Snapshot = nullptr;
  std::weak_ptr<const Data::OsmSourceSnapshot> Archive = Snapshot;
  Data::ProductOrigin Origin{};
};

}

#endif
