#ifndef OUTSHINE_TEST_RENDER_WEARS_H
#define OUTSHINE_TEST_RENDER_WEARS_H

#include <cstdint>
#include <vector>

#include <Geometry.h>
#include <Material.h>

#include "Handed.h"

namespace outshine::Test {

// WHICH SURFACE EACH PART WEARS, PREDICTED. The engine gives one SLOT to each distinct material a
// subject's parts name, and a scorer that wants to say which surface a pixel belongs to has to
// know that mapping. It used to borrow the engine's own resolved table, which is a scorer reading
// its answer off the thing it is scoring; the door states enough now that this can be worked out
// from the declaration -- a `Material` names its maps and a `Geometry` carries the images.
enum class ColourFrom { Declared, BaseColour, Emissive, Row };
enum class ColourCarrier { Texture, Factor, VertexColour };

struct Wears {
  std::vector<Material> Slots;
  std::vector<int> Material;
  std::vector<uint32_t> PartSlot;

  void Reads(const Handed &model, bool carriesTransmission, bool ownMaterials) {
    Slots.clear();
    Material.clear();
    PartSlot.assign(model.Parts().size(), 0);
    for (size_t part = 0; part < model.Parts().size(); ++part) {
      const int material = model.Parts()[part].Material;
      size_t slot = Material.size();
      for (size_t at = 0; at < Material.size(); ++at) {
        if (Material[at] == material) {
          slot = at;
          break;
        }
      }
      if (slot == Material.size()) {
        outshine::Material row;
        if (material >= 0 && (size_t)material < model.Surfaces().size()) {
          row = model.Surfaces()[(size_t)material];
          if (!carriesTransmission) {
            row.Transmission = 0.0f;
            row.Thickness = 0.0f;
          }
          if (!ownMaterials) { row.Alpha = AlphaMode::Opaque; }
        }
        Material.push_back(material);
        Slots.push_back(row);
      }
      PartSlot[part] = (uint32_t)slot;
    }
  }
};

// A SAMPLER THAT INTERPOLATES OVER MORE THAN ONE TEXEL. A picture bound at one texel by one is a
// constant whatever the filter says, so it decides nothing and does not count.
[[nodiscard]] inline bool AnyLinearFilteredImage(const Wears &wearing, const Geometry &handed) {
  const auto interpolates = [&handed](const SurfaceMap &map) {
    if (!map.bound() || map.Samples.Magnify != Filter::Linear) { return false; }
    const ImageView held = handed.imageAt(map.Image);
    return held.stands() && (held.WidthPx > 1 || held.HeightPx > 1);
  };
  for (const outshine::Material &row : wearing.Slots) {
    if (interpolates(row.BaseColourMap) || interpolates(row.NormalMap) ||
        interpolates(row.MetalRoughMap) || interpolates(row.EmissiveMap) ||
        interpolates(row.SpecularStrengthMap) || interpolates(row.SpecularTintMap)) {
      return true;
    }
  }
  return false;
}

} // namespace outshine::Test

#endif
