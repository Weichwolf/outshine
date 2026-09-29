#ifndef OUTSHINE_WORLD_DATA_OSMSOURCESNAPSHOT_H
#define OUTSHINE_WORLD_DATA_OSMSOURCESNAPSHOT_H

#include <cstddef>
#include <vector>

#include "OsmElements.h"
#include <world/SourceProvider.h>

namespace outshine::Data {

struct OsmSourceSnapshot {
  OsmElements Elements;
  std::vector<SourceCoverage> Coverage;
  size_t SourceBytes = 0;
  double ReadMs = 0.0;
  double ParseMs = 0.0;
};

}

#endif
