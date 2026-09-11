#ifndef OUTSHINE_GENERATORS_FLORA_TREELEAF_H
#define OUTSHINE_GENERATORS_FLORA_TREELEAF_H

#include "TreeMesh.h"
#include "TreeSpecies.h"
#include <cstddef>
#include <expected>
#include <string_view>

namespace outshine::Generators {

struct LeafSimplification {
  static constexpr float kDefaultRelativeAreaError = 0.02f;
  float MaxDeviation = 0.0f;
  float MaxRelativeAreaError = kDefaultRelativeAreaError;
};

class TreeLeaf {
public:
  static constexpr int kMaximumSegments = 128;
  static constexpr int kMaximumLeaflets = 16;
  static constexpr size_t kMaximumVertices = 8192;
  static constexpr size_t kMaximumIndices = 32768;

  [[nodiscard]] static std::expected<void, std::string_view>
  Validate(const TreeSpecies::Leaf &leaf);
  [[nodiscard]] static std::expected<void, std::string_view>
  Build(const TreeSpecies::Leaf &leaf, TreeMesh &out, LeafSimplification simplification = {});
};

}
#endif
