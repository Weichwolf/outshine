#include "TreePrototype.h"

#include <cmath>

#include "TreeFoliage.h"
#include "TreeGrower.h"
#include "TreeLeaf.h"
#include "TreeMesh.h"
#include "TreeMesher.h"
#include "TreeSkeleton.h"
#include "ModelLadder.h"

namespace outshine::Generators {
namespace {

const float kLeafBaseLinear[3] = {0.0684f, 0.1072f, 0.0273f};

float SrgbToLinear(float v) {
  return v <= 0.04045f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f);
}

}

TreeLook TreePrototype::LookOf(const TreeSpecies &sp) {
  TreeLook look;
  const TreeSpecies::Shading &sh = sp.ShadingParams();
  const TreeSpecies::Leaf &lf = sp.LeafParams();
  for (int c = 0; c < 3; c++) {
    look.BarkRgb[c] = SrgbToLinear(sh.BarkColor[c]);
    look.LeafRgb[c] = kLeafBaseLinear[c] * sh.LeafTint[c];
  }
  look.BarkDark = sh.BarkDark;
  look.BarkFreq = sh.BarkFreq;
  look.BarkRidge = sh.BarkRidge;
  look.LeafWidth = lf.Width;
  look.LeafWidest = lf.Widest;
  look.LeafTip = lf.Tip;
  look.LeafBaseFill = lf.BaseFill;
  look.LeafLobes = (float)lf.Lobes;
  look.LeafLobeDepth = lf.LobeDepth;
  look.LeafSerration = lf.Serration;
  look.NeedleWidth = lf.Kind == TreeSpecies::LeafKind::Needle ? lf.NeedleWidth : 0.0f;
  return look;
}

void TreePrototype::MaterialRow(const TreeLook &look, float out[kMaterialRowFloats]) {
  const float a = look.LeafWidest * 1.5f + 0.80f;
  float b = (1.0f - look.LeafWidest) * 1.5f + 0.95f - look.LeafTip * 1.25f;
  if (b < 0.55f) b = 0.55f;
  const float peak = std::pow(look.LeafWidest, a) * std::pow(1.0f - look.LeafWidest, b);
  for (int c = 0; c < 3; c++) out[c] = look.BarkRgb[c];
  out[3] = look.BarkDark;
  out[4] = look.BarkFreq;
  out[5] = look.BarkRidge;
  out[6] = look.NeedleWidth;
  out[7] = a;
  for (int c = 0; c < 3; c++) out[8 + c] = look.LeafRgb[c];
  out[11] = b;
  out[12] = look.LeafWidth;
  out[13] = look.LeafBaseFill;
  out[14] = look.LeafLobes;
  out[15] = look.LeafLobeDepth;
  out[16] = look.LeafSerration;
  out[17] = peak > 1.0e-6f ? 1.0f / peak : 0.0f;
  out[18] = 0.0f;
  out[19] = 0.0f;
}

std::optional<TreePrototype> TreePrototype::Grow(const TreeSpecies &sp) {
  TreeMesh mesh;
  TreeSkeleton plant;
  TreeGrower grower;
  TreeMesher mesher;
  TreeFoliage foliage;
  TreePrototype proto;
  proto.HeightM_ = (double)sp.HeightM();
  proto.Look_ = LookOf(sp);
  proto.Ranks_.resize((size_t)ModelLadder::kLevels);
  double crownProjM2 = 0.0;

  grower.Grow(sp, plant);
  TreeLeaf::Build(sp.LeafParams(), mesh);
  for (int rank = 0; rank < ModelLadder::kLevels; ++rank) {
    Rank &out = proto.Ranks_[(size_t)rank];
    mesher.Draw(plant, ModelLadder::Error(rank), mesh);
    foliage.Build(plant, mesh, sp, 1);
    const uint32_t stride = (uint32_t)kElementsPerSheet << (2u * (unsigned)rank);
    for (size_t i = 0; i < foliage.Count(); i += stride) {
      const float *c = &foliage.Instances()[i * TreeFoliage::kFloats];
      out.Cards.insert(out.Cards.end(), c, c + TreeFoliage::kFloats);
    }
    const uint32_t nCards = (uint32_t)(out.Cards.size() / TreeFoliage::kFloats);
    if (rank == 0) crownProjM2 = foliage.CrownProjM2();
    out.CardCount = nCards;
    out.CardLeafM = foliage.CardLeafM(kElementsPerSheet, nCards, (double)sp.Lai(), crownProjM2);
    out.BarkVerts = mesh.BarkVerts;
    out.BarkVertCount = (uint32_t)mesh.BarkVertexCount();
    out.BarkIdx = mesh.BarkIdx;
  }
  proto.Crown_.HalfWidth = std::fmax(std::fmax(-plant.BoxMin.X, plant.BoxMax.X),
                                     std::fmax(-plant.BoxMin.Z, plant.BoxMax.Z));
  proto.Crown_.Bottom = plant.BoxMin.Y;
  proto.Crown_.Top = plant.BoxMax.Y;
  proto.Crown_.HeightM = (float)proto.HeightM_;
  if (proto.Ranks_[0].BarkVertCount == 0) return std::nullopt;
  return proto;
}

}
