#ifndef TREEPROTOTYPE_H
#define TREEPROTOTYPE_H

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

  static void MaterialRow(const TreeLook &look, float out[kMaterialRowFloats]);

  const std::vector<Rank> &Ranks() const { return Ranks_; }
  const TreeLook &Look() const { return Look_; }
  const Crown &Reach() const { return Crown_; }
  double HeightM() const { return HeightM_; }

  static constexpr float kCardFanDeg = 110.0f;

private:
  std::vector<Rank> Ranks_;
  TreeLook Look_;
  Crown Crown_;
  double HeightM_ = 0.0;
};

}
#endif
