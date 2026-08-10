/* THE RESULT OF ONE SPECIES DECLARATION: the bark mesh, the species' single leaf, the leaf points and
 * the box. Delivered NORMALISED — base at y = 0, centred in x/z, height exactly 1 — so the only number
 * that carries a metre is TreeSpecies::HeightM. */
#ifndef TREEMESH_H
#define TREEMESH_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include "TreeVec3.h"

namespace outshine::World {

class TreeMesh {
public:
  static constexpr int kBarkFloats = 11; /* pos(3) nrm(3) uv(2) tan(3) */
  static constexpr int kLeafFloats = 8;  /* pos(3) nrm(3) uv(2) */

  /* Where a leaf sits on the shoot and where its stalk points. Not geometry — the population the leaf
   * angle distribution is measured over. */
  struct LeafPoint {
    TreeVec3 Pos, Dir;
  };

  std::vector<float> BarkVerts;
  std::vector<uint32_t> BarkIdx;
  std::vector<float> LeafVerts;
  std::vector<uint32_t> LeafIdx;
  std::vector<LeafPoint> LeafPoints;
  TreeVec3 BoxMin, BoxMax;
  /* Trunk radii as a FRACTION OF THE TREE'S HEIGHT, so metres = value x TreeSpecies::HeightM. The
   * grown mesh's own answer to the one number forestry measures, and what the calibration in
   * TreeGrower drives. */
  float FootRadius = 0.0f;
  float DbhRadius = 0.0f;

  size_t BarkVertexCount() const { return BarkVerts.size() / kBarkFloats; }
  size_t LeafVertexCount() const { return LeafVerts.size() / kLeafFloats; }
  size_t Bytes() const {
    return BarkVerts.size() * sizeof(float) + BarkIdx.size() * sizeof(uint32_t) +
           LeafVerts.size() * sizeof(float) + LeafIdx.size() * sizeof(uint32_t) +
           LeafPoints.size() * sizeof(LeafPoint);
  }

  /* Keeps the capacity: growing a stand reuses one mesh per species. */
  void Clear() {
    BarkVerts.clear();
    BarkIdx.clear();
    LeafVerts.clear();
    LeafIdx.clear();
    LeafPoints.clear();
    BoxMin = TreeVec3{};
    BoxMax = TreeVec3{};
    FootRadius = 0.0f;
    DbhRadius = 0.0f;
  }
};

} // namespace outshine::World
#endif
