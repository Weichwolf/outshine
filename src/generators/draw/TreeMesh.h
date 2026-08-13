/* WHAT ONE DRAWING OF A PLANT IS: the bark mesh at one budget and the species' single leaf. What the
 * plant IS — its leaf points, its box and its trunk radii — is TreeSkeleton's and is the same at every
 * budget, which is why none of it is here.
 *
 * Delivered NORMALISED, in the skeleton's own frame: origin at the trunk foot, extent 1 along the axis
 * `height_m` is measured on, so the only number that carries a metre is TreeSpecies::HeightM. */
#ifndef TREEMESH_H
#define TREEMESH_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include "ChunkVtx.h"

namespace outshine::Generators {

class TreeMesh {
public:
  static constexpr int kBarkFloats = (int)(kPlainVertexStrideB / sizeof(float));
  static constexpr int kLeafFloats = 8;  /* pos(3) nrm(3) uv(2) */

  std::vector<float> BarkVerts;
  std::vector<uint32_t> BarkIdx;
  std::vector<float> LeafVerts;
  std::vector<uint32_t> LeafIdx;

  size_t BarkVertexCount() const { return BarkVerts.size() / kBarkFloats; }
  size_t LeafVertexCount() const { return LeafVerts.size() / kLeafFloats; }
  size_t Bytes() const {
    return BarkVerts.size() * sizeof(float) + BarkIdx.size() * sizeof(uint32_t) +
           LeafVerts.size() * sizeof(float) + LeafIdx.size() * sizeof(uint32_t);
  }

  /* Keeps the capacity: drawing a stand reuses one mesh per species. The leaf is not cleared — it is
   * the species', not the budget's, and TreeLeaf writes it once. */
  void ClearBark() {
    BarkVerts.clear();
    BarkIdx.clear();
  }
  void Clear() {
    ClearBark();
    LeafVerts.clear();
    LeafIdx.clear();
  }
};

} // namespace outshine::Generators
#endif
