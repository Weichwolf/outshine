/* AN EXACT RAY AGAINST A TRIANGLE SOUP, and the acceleration structure that makes it affordable.
 * The question it answers is BINARY -- is anything between these two points -- because that is the
 * question a delta light asks and the only one a shadow needs.
 *
 * IT IS STACKLESS, AND THAT IS THE WHOLE REASON FOR THE ESCAPE FIELD. A fragment shader tracing one
 * ray per pixel pays for a traversal stack in registers: a 32-entry stack is 128 bytes a thread and
 * it comes straight out of occupancy. A depth-first node order gives the left child for free at
 * `at + 1`, so the only link a traversal needs is where to go when a box is MISSED -- and with that
 * one field there is no stack at all and no depth the structure can exceed. Front-to-back ordering
 * is what is given up, and an any-hit query is exactly the query that does not want it.
 *
 * THE NODE IS 32 BYTES BECAUSE THAT IS WHAT THE TRAVERSAL READS PER STEP. Two boxes and two links
 * spelled out would be 36 and would straddle a cache line every ninth node; the leaf payload is
 * therefore one word with the count above the first index, and the shader is spliced with the very
 * constants below so the packing has one spelling and not two.
 *
 * COORDINATES ARE THE MESH'S OWN, whatever frame it was handed in, and the structure states no
 * frame of its own. A ray is asked in the same frame -- for a subject that is anchor-relative
 * metres, which is what keeps a float exact next to an ECEF double. */
#ifndef TRIANGLEBVH_H
#define TRIANGLEBVH_H

#include <cstdint>
#include <vector>

#include "Span.h"

namespace outshine {

/* The leaf payload's two fields inside one word. 24 bits of first index is 16.7 M triangles and the
 * build refuses a mesh past it by name; 8 bits of count is far more than any leaf this build emits.
 * `kBvhInterior` is a count of zero, which no leaf can be. */
constexpr uint32_t kBvhLeafFirstBits = 24;
constexpr uint32_t kBvhLeafFirstMask = (1u << kBvhLeafFirstBits) - 1u;
constexpr uint32_t kBvhInterior = 0u;
/* The escape of the last node on any path: the traversal has run out of tree. */
constexpr uint32_t kBvhNoEscape = 0xFFFFFFFFu;
/* THE RUN BELOW WHICH NO SPLIT IS EVEN TRIED. [SET] 4: the intersection cost of a Moller-Trumbore
 * test is a few times a slab test, so a leaf wide enough to amortise a node fetch and narrow enough
 * not to test triangles a split would have rejected. It is a floor and not a ceiling -- the surface
 * area heuristic refuses a split that costs more than not splitting, and such a leaf is wider. */
constexpr uint32_t kBvhLeafTriangles = 4;

struct BvhNode {
  float MinM[3] = {0, 0, 0};
  /* Where the traversal goes when this node's box is missed, or when a leaf is done. */
  uint32_t Escape = kBvhNoEscape;
  float MaxM[3] = {0, 0, 0};
  /* `count << kBvhLeafFirstBits | first`, and `kBvhInterior` where this node has children. An
   * interior node's first child is always the next node, which is what the depth-first order buys. */
  uint32_t Leaf = kBvhInterior;

  [[nodiscard]] bool IsLeaf() const { return Leaf != kBvhInterior; }
  [[nodiscard]] uint32_t FirstTriangle() const { return Leaf & kBvhLeafFirstMask; }
  [[nodiscard]] uint32_t TriangleCount() const { return Leaf >> kBvhLeafFirstBits; }
};

/* ONE TRIANGLE IN THE ALPHABET THE INTERSECTION TEST READS, not in the mesh's. Moller-Trumbore
 * takes a vertex and two edges, so storing three vertices would make every test subtract the same
 * two vectors again -- 1.5 M times per query set, for a structure that is built once. */
struct BvhTriangle {
  float V0[3] = {0, 0, 0};
  float E1[3] = {0, 0, 0};
  float E2[3] = {0, 0, 0};
};

/* THE NODE IS THIRTY-TWO BYTES AND THE SHADER READS IT AS THIRTY-TWO BYTES. This is what makes the
 * spliced MSL declaration and this one the same layout rather than two that agree by inspection. */
static_assert(sizeof(BvhNode) == 32, "the traversal reads a 32-byte node");
static_assert(sizeof(BvhTriangle) == 36, "the intersection test reads a 36-byte triangle");

class TriangleBvh {
public:
  /* THE ONLY WAY ONE EXISTS, and it hands back a finished object (`C.41`). An empty mesh, an index
   * run that is not a multiple of three, and a mesh past the 16.7 M triangles the leaf word can
   * address all yield the EMPTY structure, which every ray misses -- a valid state, not a failure,
   * because "nothing occludes" is exactly what an absent subject means to a light. */
  [[nodiscard]] static TriangleBvh Over(Span<const float> positionsM, Span<const uint32_t> indices);

  /* IS ANYTHING STRICTLY BETWEEN THE ORIGIN AND `distanceM` ALONG `direction`. The direction need
   * not be normalised; `distanceM` is measured in units of it. `nearM` is the ray's own start,
   * which is where a surface keeps itself out of its own shadow. */
  [[nodiscard]] bool Occludes(const float originM[3], const float direction[3], float nearM,
                              float distanceM) const;

  [[nodiscard]] Span<const BvhNode> Nodes() const {
    return Span<const BvhNode>(Nodes_.data(), Nodes_.size());
  }
  [[nodiscard]] Span<const BvhTriangle> Triangles() const {
    return Span<const BvhTriangle>(Tris_.data(), Tris_.size());
  }
  /* **THE SAME TREE OVER A SUBJECT THAT MOVED** (board:1464). A pose keeps every triangle's index and
   * only changes where its corners are, so the topology, the split planes and the ordering this
   * structure was built with are all still the ones a rebuild would produce for the same triangles --
   * what is stale is the corner data and every box that contained it. **Refit rewrites both and takes
   * nothing from the allocator**: the triangle array is written through the permutation the build
   * already recorded, and the nodes are widened leaves-up, which a reverse walk of a depth-first array
   * is by construction.
   *
   * **WHAT IT COSTS IS BOX QUALITY AND THE COST IS REAL.** A tree split for one pose has looser boxes
   * over a very different one, so a traversal visits more nodes; it does not visit the WRONG ones --
   * every box still contains its children, so no query can miss a triangle. A consumer whose subject
   * deforms far from its build pose rebuilds, and when to do that is the consumer's declaration.
   *
   * Refuses a call whose index run is not the one the tree was built over, because a refit is only
   * correct while the triangles are the same triangles. */
  [[nodiscard]] bool Refit(Span<const float> positionsM, Span<const uint32_t> indices);

  [[nodiscard]] bool Empty() const { return Nodes_.empty(); }
  /* The longest root-to-leaf path, published because it is what a stacked traversal would have had
   * to size itself against -- and what this one does not. */
  [[nodiscard]] uint32_t Depth() const { return Depth_; }

private:
  std::vector<BvhNode> Nodes_;
  std::vector<BvhTriangle> Tris_;
  /* WHICH MESH TRIANGLE EACH ENTRY OF `Tris_` IS, kept because a refit has to read the same corners
   * the build did and the build reordered them. It is the permutation, not a copy of the mesh. */
  std::vector<uint32_t> Order_;
  uint32_t Depth_ = 0;
};

} // namespace outshine
#endif
