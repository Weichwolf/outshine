#ifndef OUTSHINE_BASE_SPATIAL_COOKED_H
#define OUTSHINE_BASE_SPATIAL_COOKED_H

#include <string>

#include <Geometry.h>

#include "ClusterDag.h"

namespace outshine {

struct CookedPart {
  ClusterDag Dag;
  bool HasNormals = false;
  bool HasTexture = false;
  bool HasTangents = false;
  bool HasColours = false;
};

[[nodiscard]] bool Cook(const Geometry &what, int part, const ClusterDagOpts &how,
                        CookedPart &into, std::string &error);

}
#endif
