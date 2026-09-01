#include "TreePrototype.h"
#include "Vec3.h"

#include <cmath>
#include <optional>
#include <cstddef>
#include <cstdint>

#include "TreeFoliage.h"
#include "TreeGrower.h"
#include "TreeLeaf.h"
#include "TreeMesh.h"
#include "TreeMesher.h"
#include "TreeSkeleton.h"
#include "ModelLadder.h"

namespace outshine::Generators {
namespace {

const Vec3f kLeafBaseLinear = {{0.0684f, 0.1072f, 0.0273f}};

float SrgbToLinear(float v) {
  return v <= 0.04045f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f);
}

} // namespace

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
  look.LeafLobes = static_cast<float>(lf.Lobes);
  look.LeafLobeDepth = lf.LobeDepth;
  look.LeafSerration = lf.Serration;
  look.NeedleWidth = lf.Kind == TreeSpecies::LeafKind::Needle ? lf.NeedleWidth : 0.0f;
  return look;
}

void TreePrototype::MaterialRow(const TreeLook &look, float out[kMaterialRowFloats]) {
  constexpr float kModeSlope = 1.5f;
  constexpr float kNearFloor = 0.80f;
  constexpr float kFarFloor = 0.95f;
  constexpr float kTipSharpen = 1.25f;
  constexpr float kLeastExponent = 0.55f;
  const float a = look.LeafWidest * kModeSlope + kNearFloor;
  float b = (1.0f - look.LeafWidest) * kModeSlope + kFarFloor - look.LeafTip * kTipSharpen;
  if (b < kLeastExponent) { b = kLeastExponent; }
  const float peak = std::pow(look.LeafWidest, a) * std::pow(1.0f - look.LeafWidest, b);
  for (int c = 0; c < 3; c++) { out[Row::BarkRgb + c] = look.BarkRgb[c]; }
  out[Row::BarkDark] = look.BarkDark;
  out[Row::BarkFreq] = look.BarkFreq;
  out[Row::BarkRidge] = look.BarkRidge;
  out[Row::NeedleWidth] = look.NeedleWidth;
  out[Row::LeafShapeNear] = a;
  for (int c = 0; c < 3; c++) { out[Row::LeafRgb + c] = look.LeafRgb[c]; }
  out[Row::LeafShapeFar] = b;
  out[Row::LeafWidth] = look.LeafWidth;
  out[Row::LeafBaseFill] = look.LeafBaseFill;
  out[Row::LeafLobes] = look.LeafLobes;
  out[Row::LeafLobeDepth] = look.LeafLobeDepth;
  out[Row::LeafSerration] = look.LeafSerration;
  out[Row::LeafPeakInverse] = peak > 1.0e-6f ? 1.0f / peak : 0.0f;
  for (int c = Row::RowSpare; c < Row::RowFloats; ++c) { out[c] = 0.0f; }
}

std::optional<TreePrototype> TreePrototype::Grow(const TreeSpecies &sp) {
  TreeMesh mesh;
  TreeSkeleton plant;
  TreeGrower grower;
  TreeMesher mesher;
  TreeFoliage foliage;
  TreePrototype proto;
  proto.HeightM_ = static_cast<double>(sp.HeightM());
  proto.Look_ = LookOf(sp);
  proto.Ranks_.resize(static_cast<size_t>(ModelLadder::kLevels));
  double crownProjM2 = 0.0;

  grower.Grow(sp, plant);
  TreeLeaf::Build(sp.LeafParams(), mesh);
  for (int rank = 0; rank < ModelLadder::kLevels; ++rank) {
    Rank &out = proto.Ranks_[static_cast<size_t>(rank)];
    mesher.Draw(plant, ModelLadder::Error(rank), mesh);
    foliage.Build(plant, mesh, sp, 1);
    const uint32_t stride = static_cast<uint32_t>(kElementsPerSheet)
                            << (2u * static_cast<unsigned>(rank));
    for (size_t i = 0; i < foliage.Count(); i += stride) {
      const float *c = &foliage.Instances()[i * TreeFoliage::kFloats];
      out.Cards.insert(out.Cards.end(), c, c + TreeFoliage::kFloats);
    }
    const auto nCards = static_cast<uint32_t>(out.Cards.size() / TreeFoliage::kFloats);
    if (rank == 0) { crownProjM2 = foliage.CrownProjM2(); }
    out.CardCount = nCards;
    out.CardLeafM =
        foliage.CardLeafM(kElementsPerSheet, nCards, static_cast<double>(sp.Lai()), crownProjM2);
    out.BarkVerts = mesh.BarkVerts;
    out.BarkVertCount = static_cast<uint32_t>(mesh.BarkVertexCount());
    out.BarkIdx = mesh.BarkIdx;
  }
  proto.Crown_.HalfWidth = std::fmax(std::fmax(-plant.BoxMin[0], plant.BoxMax[0]),
                                     std::fmax(-plant.BoxMin[2], plant.BoxMax[2]));
  proto.Crown_.Bottom = plant.BoxMin[1];
  proto.Crown_.Top = plant.BoxMax[1];
  proto.Crown_.HeightM = static_cast<float>(proto.HeightM_);
  if (proto.Ranks_[0].BarkVertCount == 0) { return std::nullopt; }
  return proto;
}

} // namespace outshine::Generators
