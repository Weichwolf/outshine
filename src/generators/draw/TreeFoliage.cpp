#include "TreeFoliage.h"

#include <cmath>

#include "TreeRandom.h"

namespace outshine::Generators {

namespace {

constexpr float kTau = 6.2831853f;

constexpr float kGolden = 2.39996323f;

}

void TreeFoliage::Build(const TreeSkeleton &plant, const TreeMesh &shape, const TreeSpecies &species,
                        int mult) {
  const TreeSpecies::Leaf &leaf = species.LeafParams();
  Inst_.clear();
  AreaM2_ = 0.0;
  PerPoint_ = 0.0;
  const float len = leaf.Length > 1.0e-4f ? leaf.Length : 1.0f;
  ScaleM_ = leaf.CardH > 0.0f ? leaf.CardH / len : 0.1f;

  double lamina = 0.0;
  for (size_t i = 0; i + 2 < shape.LeafIdx.size(); i += 3) {
    const float *a = &shape.LeafVerts[(size_t)shape.LeafIdx[i] * TreeMesh::kLeafFloats];
    const float *b = &shape.LeafVerts[(size_t)shape.LeafIdx[i + 1] * TreeMesh::kLeafFloats];
    const float *c = &shape.LeafVerts[(size_t)shape.LeafIdx[i + 2] * TreeMesh::kLeafFloats];
    const double e0[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    const double e1[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
    const double cx = e0[1] * e1[2] - e0[2] * e1[1];
    const double cy = e0[2] * e1[0] - e0[0] * e1[2];
    const double cz = e0[0] * e1[1] - e0[1] * e1[0];
    lamina += 0.5 * std::sqrt(cx * cx + cy * cy + cz * cz);
  }
  LocalArea_ = lamina;

  const size_t points = plant.LeafPoints.size();
  const double oneM2 = lamina * (double)ScaleM_ * (double)ScaleM_;
  const double h = (double)species.HeightM();
  CrownProjM2_ = 0.25 * 3.14159265358979 * (double)(plant.BoxMax.X - plant.BoxMin.X) * h *
                 (double)(plant.BoxMax.Z - plant.BoxMin.Z) * h;
  if (points == 0 || oneM2 <= 0.0) { return; }
  double want = (double)species.Lai() * CrownProjM2_ / oneM2;
  want *= (double)(mult > 0 ? mult : 1);

  if (want > (double)kMaxInstances) { want = (double)kMaxInstances; }
  PerPoint_ = want > 0.0 ? want / (double)points
                         : (double)leaf.CardsPerPoint * (double)(mult > 0 ? mult : 1);
  if (PerPoint_ <= 0.0) { return; }
  Inst_.reserve((size_t)(PerPoint_ * (double)points + 1.0) * kFloats);

  TreeRandom rng(0x1eaf0001u);
  double owed = 0.0;
  for (const LeafPoint &p : plant.LeafPoints) {

    owed += PerPoint_;
    const long n = (long)(owed + 0.5);
    owed -= (double)n;
    for (long k = 0; k < n; ++k) {
      const float roll = kGolden * (float)Inst_.size() / (float)kFloats + rng.Signed() * 0.35f;
      Inst_.insert(Inst_.end(), {p.Pos.X, p.Pos.Y, p.Pos.Z, std::fmod(roll, kTau),
                                 p.Dir.X, p.Dir.Y, p.Dir.Z, 0.0f});
    }
  }
  AreaM2_ = oneM2 * (double)Count();
}

float TreeFoliage::CardLeafM(int leavesPerCard, size_t cards, double lai,
                             double crownProjM2) const {
  const double per = (double)(leavesPerCard > 0 ? leavesPerCard : 1) * (double)cards * LocalArea_;
  if (per <= 0.0 || lai <= 0.0 || crownProjM2 <= 0.0) { return ScaleM_; }
  return (float)std::sqrt(lai * crownProjM2 / per);
}

}
