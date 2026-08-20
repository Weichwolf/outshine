#ifndef TREELEAF_H
#define TREELEAF_H

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
