#ifndef OUTSHINE_BASE_SPATIAL_CLUSTERCOOK_H
#define OUTSHINE_BASE_SPATIAL_CLUSTERCOOK_H

#include <cstdint>
#include <span>
#include <vector>

#include "ClusterDag.h"

namespace outshine {

// WHAT A COOKER IS FOR, AND WHY IT COULD NOT BE WRITTEN BEFORE. The device cannot cull what it
// cannot address: a selection per CLUSTER needs clusters, and a subject that reaches the GPU as
// one undivided index run offers nothing to reject but the whole of it. `ClusterDag.h`'s own note
// records that a builder stood here, cooked nothing and was deleted, because every path that would
// have fed it carried the importer's double-precision carrier instead of the value the device
// binds. board:1949 made that one value, so the cooker is written against THAT -- plain float
// positions in the device's own layout and a plain index run, which is what a generator writes and
// what an importer narrows to exactly once.
//
// Unreal cooks its DAG OFFLINE at import and RAGE cooks a drawable offline too; neither clusters
// at runtime. This one runs at runtime because this world is GENERATED at runtime and there is no
// import step to cook in -- and that is a difference in the CONTENT rather than a disagreement
// about the technique, so the shape of the answer is still theirs.
//
// THE TERRAIN IS NOT COOKED AND MUST NOT BE. A tile cascade already carries its own pyramid: zoom
// z-1 IS the simplified z, produced by whoever made the tiles. `CookedTile` records the
// measurement that settled it -- 572 of the stack samples went into clustering terrain that was
// already reduced. This cooker is for SUBJECTS, which have no natural pyramid, and that is exactly
// where Nanite uses one.
struct Cooked {
  std::vector<DagCluster> Clusters;
  std::vector<uint32_t> Index;

  // THE COARSER LEVELS' OWN VERTICES, APPENDED AFTER THE INPUT'S. Level 0 indexes the positions it
  // was handed; every level above indexes vertices this cooker MADE, so they have to travel with
  // it. `FirstOwnVertex` is where the input stops and the cooker's own begin, which is what a
  // caller needs to upload one buffer instead of two.
  std::vector<float> PositionsM;
  uint32_t FirstOwnVertex = 0;
};

// THE CUT IS BY LOCALITY AND NOTHING CLEVERER, and the reason is that the next stage does not need
// cleverer yet. A cluster is rejected by a sphere, so what a cut has to produce is TIGHT spheres,
// and triangles that sit near each other produce them. Sorting by the Morton code of the centroid
// interleaves the axes so that a run of the sorted order is a box rather than a slab -- the same
// reason a Morton order is used for a BVH build, and the same reason meshlet builders start there.
//
// EVERY TRIANGLE LANDS IN EXACTLY ONE CLUSTER. That is the invariant this is worth proving: a cut
// that drops one draws a hole, and a cut that repeats one draws it twice and costs twice. It does
// not depend on the cut being GOOD, which is what makes it a fair test of a cut that will change.
[[nodiscard]] Cooked CookClusters(std::span<const float> positionsM,
                                  std::span<const uint32_t> indices,
                                  uint32_t mostTriangles);

// THE PARENT ERROR IS WHAT MAKES IT A DAG RATHER THAN A LIST. `DagSelect` keeps a cluster when its
// OWN error is small enough on screen and its PARENT's is not -- so without a coarser level every
// cluster's parent error is the root and every cluster draws, whatever the distance. That is the
// flat cut, and it is enough to cull with and not enough to choose a level with.
//
// THE SIMPLIFIER IS VERTEX CLUSTERING ON A GRID, and it is chosen for a reason a quadric cannot
// match here: its error is a BOUND rather than an estimate. Collapse every vertex in a cell to one
// representative and the displacement of any vertex is at most the distance to that representative,
// which this measures exactly and hands to the parent as its error. Unreal's Nanite uses quadric
// edge collapse over a cluster GROUP and gets a better triangle for the same error; that is the
// better answer and it is the one to take when this has a measurement to beat. A bound that is
// stated and true beats an estimate that is better and unproven, and the item says which is which.
//
// `mostLevels` counts the coarser levels ABOVE the leaves. One is a DAG; zero is `CookClusters`.
[[nodiscard]] Cooked CookDag(std::span<const float> positionsM,
                             std::span<const uint32_t> indices,
                             uint32_t mostTriangles,
                             uint32_t mostLevels);

} // namespace outshine
#endif
