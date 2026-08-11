/* ONE SPECIES GROWN ONCE, in model space and metres. It is the cluster the forest draws instances
 * of, and growing it is NOT a generator call: it happens at bring-up, before a region exists, and
 * whoever wants a picture out of it uploads it and keeps nothing else. */
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
  /* The grown mesh's box in tree heights: half width, and the bottom and top the foliage reaches.
   * What an orthographic bake has to frame, and what decides whether a lens stands inside a crown. */
  struct Crown {
    float HalfWidth = 0.0f, Bottom = 0.0f, Top = 0.0f, HeightM = 0.0f;
  };
  struct Rank {
    std::vector<float> BarkVerts;     /* core/ChunkVtx.h layout */
    uint32_t BarkVertCount = 0;
    std::vector<uint32_t> BarkIdx;
    std::vector<float> Cards;         /* TreeFoliage::kFloats per card */
    uint32_t CardCount = 0;
    float CardLeafM = 0.0f;           /* the leaf a card draws, edge length in metres */
  };

  /* Empty where the species will not grow a mesh at all. */
  static std::optional<TreePrototype> Grow(const TreeSpecies &species);
  /* A declared colour is sRGB and a reflectance is linear; the leaf tint multiplies the table's own
   * fresh-blade green. Both conversions belong to the species, so render/ never sees a declaration. */
  static TreeLook LookOf(const TreeSpecies &species);
  /* THE MATERIAL ROW the picture takes, filled in the order the shader that reads it declares. The
   * outline profile's exponents are derived HERE and not there: the shape of a lamina is species
   * knowledge, and a renderer that computed it would be holding one. */
  static void MaterialRow(const TreeLook &look, float out[kMaterialRowFloats]);

  const std::vector<Rank> &Ranks() const { return Ranks_; }
  const TreeLook &Look() const { return Look_; }
  const Crown &Reach() const { return Crown_; }
  double HeightM() const { return HeightM_; }
  /* [SET] The fan a shoot's leaves stand in about their stalk, full angle. A card is one shoot tip,
   * and a rosette of four laminae over less than a right angle stacks into a line from every
   * direction but one. */
  static constexpr float kCardFanDeg = 110.0f;

private:
  std::vector<Rank> Ranks_;
  TreeLook Look_;
  Crown Crown_;
  double HeightM_ = 0.0;
};

} // namespace outshine::Generators
#endif
