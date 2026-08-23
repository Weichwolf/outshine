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
  static constexpr int kLeafFloats = 8;

  std::vector<float> BarkVerts;
  std::vector<uint32_t> BarkIdx;
  std::vector<float> LeafVerts;
  std::vector<uint32_t> LeafIdx;

  [[nodiscard]] size_t BarkVertexCount() const { return BarkVerts.size() / kBarkFloats; }
  [[nodiscard]] size_t LeafVertexCount() const { return LeafVerts.size() / kLeafFloats; }
  [[nodiscard]] size_t Bytes() const {
    return BarkVerts.size() * sizeof(float) + BarkIdx.size() * sizeof(uint32_t) +
           LeafVerts.size() * sizeof(float) + LeafIdx.size() * sizeof(uint32_t);
  }

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

}
#endif
