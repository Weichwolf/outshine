#ifndef OUTSHINE_GENERATORS_DRAW_TREELEAF_H
#define OUTSHINE_GENERATORS_DRAW_TREELEAF_H

#include "TreeMesh.h"
#include "TreeSpecies.h"
#include "TreeVec3.h"

namespace outshine::Generators {

class TreeLeaf {
public:

  static void Build(const TreeSpecies::Leaf &leaf, TreeMesh &out);
};

}
#endif
