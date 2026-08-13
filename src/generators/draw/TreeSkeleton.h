/* THE GROWN PLANT, AND A BUDGET HAS NO SPELLING IN IT. The species declaration and its seed decide
 * every number here; a level of detail is a VIEW over this (TreeMesher), never an input to it. Two
 * ranks are therefore two drawings of one plant instead of two plants, which is what makes a coarse
 * rank a SUBSET of a fine one and an LOD switch something other than a lottery.
 *
 * Delivered NORMALISED, the way TreeMesh states it: origin at the trunk foot, extent 1 along the axis
 * `height_m` is measured on. One pixel is then literally `pixelHeightFrac` in these units. */
#ifndef TREESKELETON_H
#define TREESKELETON_H

#include <cstdint>
#include <vector>

#include "TreeVec3.h"

namespace outshine::Generators {

/* Where a leaf sits on the shoot and where its stalk points. Not geometry — the population the leaf
 * angle distribution is measured over, and it belongs to the growth, so it is the same at every rank. */
struct LeafPoint {
  TreeVec3 Pos, Dir;
};

/* How a shoot's open end is closed. The base ring faces the other way; a live tip runs out to a
 * point; a cut is what a saw or a shear leaves; a break is what wind leaves. */
enum class RingCap : uint8_t { Base, Point, Cut, Broken };

class TreeSkeleton {
public:
  /* One station along a shoot's axis: where it is, where it points, the frame its ring is built in
   * and how thick it is there. */
  struct Node {
    TreeVec3 Pos, Dir, Up;
    float Radius = 0.0f;
  };

  /* A run of nodes with one parent. Node `First` is the anchor — the seed for a leader, a point on
   * the parent's own surface for a branch — and the drawn tube runs from it to `First + Count - 1`. */
  struct Shoot {
    int Parent = -1;      /* index into Shoots, < 0 for a leader */
    int ParentNode = -1;  /* the parent node whose incoming ring band carries this shoot */
    float Roll = 0.0f;    /* where round that band, measured from the band's own Up */
    int First = 0, Count = 0;
    int Sides = 3;        /* the declaration's own ceiling; the pixel rule works under it */
    RingCap End = RingCap::Point;
    /* HOW FAR THIS SHOOT AND EVERYTHING IT CARRIES REACHES FROM ITS OWN ANCHOR, and never less than
     * a child's. Dropping a shoot leaves an error under this number, so a drop bounded by it is
     * bounded in the same pixels the side count is. */
    float Reach = 0.0f;
  };

  std::vector<Node> Nodes;
  std::vector<Shoot> Shoots;
  std::vector<LeafPoint> LeafPoints;

  /* The box over the whole plant — every node's disc, every cap and every leaf point — which is what
   * the normalisation above divides by. `BoxMax.Y` is 1 for a standing form and the larger horizontal
   * run is 1 for a lying one; `BoxMin.Y` may be negative and a branch below it grows below the
   * terrain. */
  TreeVec3 BoxMin, BoxMax;
  /* Trunk radii as a fraction of the plant's height, so metres = value x TreeSpecies::HeightM. */
  float FootRadius = 0.0f;
  float DbhRadius = 0.0f;
  /* The declaration's own seed, so a drawing can be random without being random per rank. */
  uint32_t Seed = 1;

  /* Keeps the capacity: growing a stand reuses one skeleton per species. */
  void Clear() {
    Nodes.clear();
    Shoots.clear();
    LeafPoints.clear();
    BoxMin = TreeVec3{};
    BoxMax = TreeVec3{};
    FootRadius = 0.0f;
    DbhRadius = 0.0f;
  }
};

} // namespace outshine::Generators
#endif
