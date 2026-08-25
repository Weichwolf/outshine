#ifndef OUTSHINE_WORLD_GENERATORS_DRAW_TREEFOLIAGE_H
#define OUTSHINE_WORLD_GENERATORS_DRAW_TREEFOLIAGE_H

#include <cstddef>
#include <vector>

#include "TreeMesh.h"
#include "TreeSkeleton.h"
#include "TreeSpecies.h"

namespace outshine::Generators {

class TreeFoliage {
public:
  static constexpr int kFloats = 8;

  static constexpr int kMaxInstances = 1000000;

  void Build(const TreeSkeleton &plant, const TreeMesh &leaf, const TreeSpecies &species,
             int mult = 1);

  [[nodiscard]] const std::vector<float> &Instances() const { return Inst_; }
  [[nodiscard]] size_t Count() const { return Inst_.size() / kFloats; }

  [[nodiscard]] double PerPoint() const { return PerPoint_; }
  [[nodiscard]] double CrownProjM2() const { return CrownProjM2_; }

  [[nodiscard]] float ScaleM() const { return ScaleM_; }

  [[nodiscard]] double LeafAreaM2() const { return AreaM2_; }
  [[nodiscard]] double OneLeafAreaM2() const { return Count() > 0 ? AreaM2_ / (double)Count() : 0.0; }

  [[nodiscard]] double LaminaAreaLocal() const { return LocalArea_; }

  [[nodiscard]] float CardLeafM(int leavesPerCard, size_t cards, double lai, double crownProjM2) const;

private:
  std::vector<float> Inst_;
  float ScaleM_ = 0.1f;
  double AreaM2_ = 0.0;
  double LocalArea_ = 0.0;
  double PerPoint_ = 0.0;
  double CrownProjM2_ = 0.0;
};

}
#endif
