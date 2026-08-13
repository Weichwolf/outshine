/* ONE PLANT, DRAWN AT ONE BUDGET. The skeleton is const here and that is the whole point: a rank
 * SELECTS from a grown plant and cannot grow a different one, so a coarse rank's shoots are a subset
 * of a fine rank's and an LOD switch cannot make a tree appear, vanish or change height.
 *
 * GROWTH BY EXTRUSION on ONE closed mesh. A seed ring plus a ground cap; every node pulls the open
 * ring forward; a branch is a SIDE FACE of the parent extruded away, so trunk and branches share a
 * topology instead of intersecting. Where the face a branch wanted is gone — the parent undrawn at
 * this budget, or another branch there already — the branch is its own closed shell instead, which
 * keeps the mesh watertight without moving one vertex of the plant.
 *
 * Buffers are members and are reused: drawing a stand allocates once per mesher, not once per plant. */
#ifndef TREEMESHER_H
#define TREEMESHER_H

#include <cstdint>
#include <vector>

#include "TreeMesh.h"
#include "TreeSkeleton.h"
#include "TreeVec3.h"

namespace outshine::Generators {

class TreeMesher {
public:
  /* `pixelHeightFrac` IS THE MODEL LENGTH OF ONE PIXEL, as a fraction of the plant's own height, at
   * the nearest distance this mesh will be drawn from. The plant is delivered at height 1, so one
   * pixel is that number and nothing has to be converted.
   *
   * IT IS THE ONLY DETAIL KNOB AND ALL THREE OF ITS ANSWERS ARE PIXELS.
   *   - how many sides a tube gets: the silhouette sagitta stays under half a pixel;
   *   - how far apart its rings stand: a skipped station's axis and radius stay within half a pixel
   *     of the chord that replaces them, and the stride is halved rather than chosen freely, so a
   *     coarse rank's rings are a subset of a fine rank's;
   *   - what is left out entirely: a shoot AND EVERYTHING IT CARRIES, and only where the whole of it
   *     fits inside one pixel of the point it leaves the parent at — so what is dropped is under a
   *     pixel of silhouette by construction, however long and however thin it is.
   * Bounding the drop by a shoot's WIDTH instead cost the beech 109 px of its own outline at a
   * one-pixel budget, because a shoot one pixel wide is a whole crown long.
   *
   * 0 draws the plant entire. */
  void Draw(const TreeSkeleton &plant, float pixelHeightFrac, TreeMesh &out);

private:
  static constexpr int kMaxSides = 16;

  /* D < 0 marks a triangle. A face is never erased, only marked dead: the indices of its neighbours
   * must survive a branch replacing a wall. */
  struct Face {
    int A, B, C, D;
  };
  /* The band of faces whose upper ring is one node, and how many sides that ring has. A branch finds
   * the wall it leaves through here; `First` < 0 is a node no ring was drawn at. */
  struct Band {
    int First = -1;
    int Sides = 0;
  };

  int AddVert(TreeVec3 p);
  int AddFace(int a, int b, int c, int d);
  TreeVec3 FaceCentroid(int fi) const;
  /* A regular n-gon of radius r misses its circle by r(1 - cos(pi/n)); half a pixel of that is the
   * whole budget, and the plant's own declared side count stays the ceiling — the rule may make a
   * tube coarser, never rounder than the declaration. */
  int SidesFor(float radius, int declared) const;
  /* Which of a shoot's stations still carry a ring. The stride doubles until the chord it spans
   * leaves the axis or the radius by more than half a pixel, and it is counted back from the tip so
   * that halving the budget only ever ADDS rings. */
  void RingsOf(const TreeSkeleton &plant, const TreeSkeleton::Shoot &shoot, int from);
  [[nodiscard]] bool ChordHolds(const TreeSkeleton &plant, int from, int last, int stride) const;
  void Ring(const TreeSkeleton::Node &node, float radius, int sides, int *out);
  void Wall(const int *from, const int *to, int sides);
  void BreakProfile(uint32_t seed, int sides, float *out) const;
  void Cap(const TreeSkeleton::Node &node, const int *ring, int sides, RingCap cap, uint32_t seed);
  /* The stitch that lets a branch leave through a parent's wall without a T-junction. Answers false
   * where that wall is not available, and then the branch is drawn as its own shell. */
  [[nodiscard]] bool Collar(int face, const TreeSkeleton::Node &anchor,
                            const TreeSkeleton::Node &first, int sides, float room, int *out);
  float RoomAt(const TreeSkeleton &plant, const TreeSkeleton::Shoot &shoot) const;
  void Export(TreeMesh &out);

  float PixelGrow_ = 0.0f;
  std::vector<TreeVec3> Verts_;
  std::vector<Face> Faces_;
  std::vector<uint8_t> Dead_;
  std::vector<uint8_t> Drawn_;
  std::vector<Band> Bands_;
  std::vector<int> Stations_;
  std::vector<TreeVec3> Normals_;
};

} // namespace outshine::Generators
#endif
