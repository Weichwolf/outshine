/* THE SPECIES' SINGLE LEAF — one mesh per species, not per leaf. A 2D silhouette subdivided along the
 * midrib, then folded and curved; five kinds, because a needle shoot, a fan and a pinnate frond are not
 * one shape with parameters. Written in leaf-local space, so it is independent of the tree's scale. */
#ifndef TREELEAF_H
#define TREELEAF_H

#include "TreeMesh.h"
#include "TreeSpecies.h"
#include "TreeVec3.h"

namespace outshine::World {

class TreeLeaf {
public:
  /* Fills out.LeafVerts / out.LeafIdx; touches nothing else on the mesh. */
  static void Build(const TreeSpecies::Leaf &leaf, TreeMesh &out);
};

} // namespace outshine::World
#endif
