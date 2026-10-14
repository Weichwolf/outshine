#ifndef OUTSHINE_GENERATORS_FLORA_TREELOOK_H
#define OUTSHINE_GENERATORS_FLORA_TREELOOK_H

#include "math/Vec3.h"
#include "math/Srgb.h"
#include "TreeSpecies.h"
#include "LeafOpticsDefaults.h"

namespace outshine {

struct TreeLook {
  Vec3f BarkRgb = {
      {ColourSpace::LinearFromSrgb(Generators::TreeSpecies::kShadingUnsaid.BarkColor[0]),
       ColourSpace::LinearFromSrgb(Generators::TreeSpecies::kShadingUnsaid.BarkColor[1]),
       ColourSpace::LinearFromSrgb(Generators::TreeSpecies::kShadingUnsaid.BarkColor[2])}};
  float BarkDark = Generators::TreeSpecies::kShadingUnsaid.BarkDark;
  float BarkFreq = Generators::TreeSpecies::kShadingUnsaid.BarkFreq;
  float BarkRidge = Generators::TreeSpecies::kShadingUnsaid.BarkRidge;
  Vec3f LeafRgb = Generators::DefaultLeafReflection();
  Vec3f LeafTransmission = Generators::DefaultLeafTransmission();
  float LeafWidth = Generators::TreeSpecies::kLeafUnsaid.Width;
  float LeafWidest = Generators::TreeSpecies::kLeafUnsaid.Widest;
  float LeafTip = Generators::TreeSpecies::kLeafUnsaid.Tip;
  float LeafBaseFill = Generators::TreeSpecies::kLeafUnsaid.BaseFill;
  float LeafLobes = static_cast<float>(Generators::TreeSpecies::kLeafUnsaid.Lobes);
  float LeafLobeDepth = Generators::TreeSpecies::kLeafUnsaid.LobeDepth;
  float LeafSerration = Generators::TreeSpecies::kLeafUnsaid.Serration;
  float NeedleWidth = 0.0f;
  float BarkRoughness = Generators::TreeSpecies::kShadingUnsaid.BarkRoughness;
  float LeafRoughness = Generators::TreeSpecies::kShadingUnsaid.LeafRoughness;
};

}
#endif
