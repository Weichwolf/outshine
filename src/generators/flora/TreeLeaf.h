#ifndef OUTSHINE_GENERATORS_FLORA_TREELEAF_H
#define OUTSHINE_GENERATORS_FLORA_TREELEAF_H

#include "TreeMesh.h"
#include "TreeSpecies.h"
#include "math/Vec3.h"

namespace outshine::Generators {

class TreeLeaf {
public:
  static void Build(const TreeSpecies::Leaf &leaf,
                    TreeMesh &out,
                    float maxDeviation = 0.0f,
                    float maxRelativeAreaError = 0.02f);
};

} // namespace outshine::Generators
#endif
