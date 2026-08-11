/* THE LEAF POPULATION AS DRAWABLE INSTANCES: TreeMesh carries ONE leaf and the points it sits on, and
 * this turns the two into the array a draw call consumes. It reads the two declared leaf fields that
 * nothing read before — `leaf_cards` (how many laminae one shoot point carries) and `leaf_card_h`
 * (the leaf's length in METRES, the only metre a leaf declaration owns).
 *
 * THE ROLL IS FREE AND THAT IS NOT A LIBERTY. LeafAngleDistribution measures G(el) under exactly one
 * assumption — the lamina rolls freely about its stalk — so a renderer that tilted a leaf off its
 * stalk would draw a canopy the far stage no longer computes. Midrib on `Dir`, normal anywhere on the
 * circle perpendicular to it: the picture and the number then describe one tree. */
#ifndef TREEFOLIAGE_H
#define TREEFOLIAGE_H

#include <cstddef>
#include <vector>

#include "TreeMesh.h"
#include "TreeSpecies.h"

namespace outshine::Generators {

class TreeFoliage {
public:
  static constexpr int kFloats = 8; /* pos(3) roll(1) dir(3) pad(1) */
  /* [SET] one crown's instance buffer: 1e6 * kFloats * 4 B = 32 MB, well inside the 256 MB
   * maxBufferSize the target survey reports. */
  static constexpr int kMaxInstances = 1000000;

  /* AREA IS BOUGHT IN COUNT. The species declares its leaf's own length (`leaf_card_h`) and the leaf
   * area index its stand was measured at, so the only free quantity is HOW MANY laminae an attachment
   * point carries — `lai * crownProjection / laminaArea`, spread over the grown points. Paying the
   * index in leaf SIZE instead would draw a beech leaf the size of a plate.
   *
   * `mult` is a BENCH BRACKET on that count, the way --ev brackets the declared exposure: the crown
   * a species declares comes out at 1, and a critic who has to rule on whether the declaration is
   * right needs to see the same tree at another density. Never a scene parameter. */
  void Build(const TreeMesh &mesh, const TreeSpecies &species, int mult = 1);

  const std::vector<float> &Instances() const { return Inst_; }
  size_t Count() const { return Inst_.size() / kFloats; }
  /* Laminae per grown attachment point — THE METER ON THE WUCHS. A shoot point stands for one leaf
   * cluster, so a tree whose shoot system is honest lands between 1 and 5; a large number is a crown
   * with too few shoots, and it says so in the picture as rosettes on a bare skeleton. */
  double PerPoint() const { return PerPoint_; }
  double CrownProjM2() const { return CrownProjM2_; }
  /* Metres per unit of leaf-local length: the declared card height over the declared leaf length. */
  float ScaleM() const { return ScaleM_; }
  /* One-sided lamina area of the WHOLE crown, m^2 — the numerator of a leaf area index. */
  double LeafAreaM2() const { return AreaM2_; }
  double OneLeafAreaM2() const { return Count() > 0 ? AreaM2_ / (double)Count() : 0.0; }
  /* Area of ONE lamina at leaf-local length 1: the shape's own constant, free of any metre. */
  double LaminaAreaLocal() const { return LocalArea_; }

  /* THE LEAF LENGTH A CLUSTER CARD MUST DRAW. `leavesPerCard` laminae on each of `cards` cards have
   * to add up to the declared leaf area index over the crown's projected area, and that fixes the
   * length: lai * proj = cards * per * LocalArea * L^2. The drawn leaf comes out larger than the
   * species' own — that is the price of a closed canopy at two triangles per card, and it is a
   * DERIVED price rather than a chosen one. `cards` is not Count() because a distant rank draws a
   * THINNED set: fewer cards, each with a longer leaf, the same leaf area index. */
  float CardLeafM(int leavesPerCard, size_t cards, double lai, double crownProjM2) const;

private:
  std::vector<float> Inst_;
  float ScaleM_ = 0.1f;
  double AreaM2_ = 0.0;
  double LocalArea_ = 0.0;
  double PerPoint_ = 0.0;
  double CrownProjM2_ = 0.0;
};

} // namespace outshine::Generators
#endif
