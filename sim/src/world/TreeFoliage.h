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

namespace outshine::World {

class TreeFoliage {
public:
  static constexpr int kFloats = 8; /* pos(3) roll(1) dir(3) pad(1) */

  /* `mult` is a BENCH BRACKET on the declared `leaf_cards`, the way --ev brackets the declared
   * exposure: the crown a species declares comes out at 1, and a critic who has to rule on whether
   * the declaration is right needs to see the same tree at another density. Never a scene parameter. */
  void Build(const TreeMesh &mesh, const TreeSpecies::Leaf &leaf, int mult = 1);

  const std::vector<float> &Instances() const { return Inst_; }
  size_t Count() const { return Inst_.size() / kFloats; }
  /* Metres per unit of leaf-local length: the declared card height over the declared leaf length. */
  float ScaleM() const { return ScaleM_; }
  /* One-sided lamina area of the WHOLE crown, m^2 — the numerator of a leaf area index. */
  double LeafAreaM2() const { return AreaM2_; }
  double OneLeafAreaM2() const { return Count() > 0 ? AreaM2_ / (double)Count() : 0.0; }

private:
  std::vector<float> Inst_;
  float ScaleM_ = 0.1f;
  double AreaM2_ = 0.0;
};

} // namespace outshine::World
#endif
