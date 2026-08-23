#ifndef OUTSHINE_GENERATORS_DRAW_TREEPROTOTYPE_H
#define OUTSHINE_GENERATORS_DRAW_TREEPROTOTYPE_H

#include <cstdint>
#include <optional>
#include <vector>

#include "Material.h"
#include "TreeLook.h"
#include "TreeSpecies.h"

namespace outshine::Generators {

class TreePrototype {
public:

  struct Crown {
    float HalfWidth = 0.0f, Bottom = 0.0f, Top = 0.0f, HeightM = 0.0f;
  };
  struct Rank {
    std::vector<float> BarkVerts;
    uint32_t BarkVertCount = 0;
    std::vector<uint32_t> BarkIdx;
    std::vector<float> Cards;
    uint32_t CardCount = 0;
    float CardLeafM = 0.0f;
  };

  static std::optional<TreePrototype> Grow(const TreeSpecies &species);

  static TreeLook LookOf(const TreeSpecies &species);

  // the row's fields, named once: an index-mapped float row whose meanings lived nowhere
  // is a second spelling of a struct (board:1747)
  enum Row : int {
    BarkRgb = 0,     // 0..2
    BarkDark = 3,
    BarkFreq = 4,
    BarkRidge = 5,
    NeedleWidth = 6,
    LeafShapeNear = 7,  // the beta density's near exponent
    LeafRgb = 8,        // 8..10
    LeafShapeFar = 11,  // and its far exponent
    LeafWidth = 12,
    LeafBaseFill = 13,
    LeafLobes = 14,
    LeafLobeDepth = 15,
    LeafSerration = 16,
    LeafPeakInverse = 17,
    RowSpare = 18,      // 18..19, zeroed: the row is a fixed width the device binds
    RowFloats = 20,
  };
  static_assert((int)Row::RowFloats == kMaterialRowFloats,
                "the tree's material row IS the engine's material row -- one width, one "
                "spelling");

  static void MaterialRow(const TreeLook &look, float out[kMaterialRowFloats]);

  [[nodiscard]] const std::vector<Rank> &Ranks() const { return Ranks_; }
  [[nodiscard]] const TreeLook &Look() const { return Look_; }
  [[nodiscard]] const Crown &Reach() const { return Crown_; }
  [[nodiscard]] double HeightM() const { return HeightM_; }

  static constexpr float kCardFanDeg = 110.0f;

private:
  std::vector<Rank> Ranks_;
  TreeLook Look_;
  Crown Crown_;
  double HeightM_ = 0.0;
};

}
#endif
