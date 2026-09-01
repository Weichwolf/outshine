#ifndef OUTSHINE_GENERATORS_DRAW_TREELEAF_H
#define OUTSHINE_GENERATORS_DRAW_TREELEAF_H

#include "TreeMesh.h"
#include "TreeSpecies.h"
#include "math/Vec3.h"

namespace outshine::Generators {

class TreeLeaf {
public:
  static void Build(const TreeSpecies::Leaf &leaf, TreeMesh &out);
};

} // namespace outshine::Generators
#endif
