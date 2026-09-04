#include "TreeFoliage.h"
#include "math/Vec3.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <cmath>

#include "TreeRandom.h"

namespace outshine::Generators {

constexpr float kScaleFallbackM = 0.1f;

constexpr uint32_t kFoliageSeed = 0x1eaf0001u;

constexpr double kQuarterOfPi = 0.25;

namespace {

constexpr float kTau = 2.0f * std::numbers::pi_v<float>;

constexpr float kGolden = 2.0f * std::numbers::pi_v<float> * (2.0f - std::numbers::phi_v<float>);

} // namespace

void TreeFoliage::Build(const TreeSkeleton &plant,
                        const TreeMesh &shape,
                        const TreeSpecies &species,
                        int mult) {
  const TreeSpecies::Leaf &leaf = species.LeafParams();
  Inst_.clear();
  AreaM2_ = 0.0;
  PerPoint_ = 0.0;
  const float len = leaf.Length > 1.0e-4f ? leaf.Length : 1.0f;
  ScaleM_ = leaf.CardH > 0.0f ? leaf.CardH / len : kScaleFallbackM;

  double lamina = 0.0;
  for (size_t i = 0; i + 2 < shape.LeafIdx.size(); i += 3) {
    const float *a =
        &shape.LeafVerts[static_cast<size_t>(shape.LeafIdx[i]) * TreeMesh::kLeafFloats];
    const float *b =
        &shape.LeafVerts[static_cast<size_t>(shape.LeafIdx[i + 1]) * TreeMesh::kLeafFloats];
    const float *c =
        &shape.LeafVerts[static_cast<size_t>(shape.LeafIdx[i + 2]) * TreeMesh::kLeafFloats];
    const Vec3 e0 = {{b[0] - a[0], b[1] - a[1], b[2] - a[2]}};
    const Vec3 e1 = {{c[0] - a[0], c[1] - a[1], c[2] - a[2]}};
    const double cx = e0[1] * e1[2] - e0[2] * e1[1];
    const double cy = e0[2] * e1[0] - e0[0] * e1[2];
    const double cz = e0[0] * e1[1] - e0[1] * e1[0];
    lamina += 0.5 * std::sqrt(cx * cx + cy * cy + cz * cz);
  }
  LocalArea_ = lamina;

  const size_t points = plant.LeafPoints.size();
  const double oneM2 = lamina * static_cast<double>(ScaleM_) * static_cast<double>(ScaleM_);
  const auto h = static_cast<double>(species.HeightM());
  CrownProjM2_ = kQuarterOfPi * std::numbers::pi *
                 static_cast<double>(plant.BoxMax[0] - plant.BoxMin[0]) * h *
                 static_cast<double>(plant.BoxMax[2] - plant.BoxMin[2]) * h;
  if (points == 0 || oneM2 <= 0.0) { return; }
  double want = static_cast<double>(species.Lai()) * CrownProjM2_ / oneM2;
  want *= static_cast<double>(mult > 0 ? mult : 1);

  want = std::min(want, static_cast<double>(kMaxInstances));
  PerPoint_ = want > 0.0 ? want / static_cast<double>(points)
                         : static_cast<double>(leaf.CardsPerPoint) *
                               static_cast<double>(mult > 0 ? mult : 1);
  if (PerPoint_ <= 0.0) { return; }
  Inst_.reserve(static_cast<size_t>(PerPoint_ * static_cast<double>(points) + 1.0) * kFloats);

  TreeRandom rng(kFoliageSeed);
  double owed = 0.0;
  for (const LeafPoint &p : plant.LeafPoints) {
    owed += PerPoint_;
    const long n = std::lround(owed);
    owed -= static_cast<double>(n);
    for (long k = 0; k < n; ++k) {
      const float roll = kGolden * static_cast<float>(Inst_.size()) / static_cast<float>(kFloats) +
                         rng.Signed() * 0.35f;
      Inst_.insert(Inst_.end(),
                   {p.Pos[0],
                    p.Pos[1],
                    p.Pos[2],
                    std::fmod(roll, kTau),
                    p.Dir[0],
                    p.Dir[1],
                    p.Dir[2],
                    0.0f});
    }
  }
  AreaM2_ = oneM2 * static_cast<double>(Count());
}

float TreeFoliage::CardLeafM(Sheets over, Canopy under) const {
  const int leavesPerCard = over.LeavesPerCard;
  const size_t cards = over.Cards;
  const double lai = under.Lai;
  const double crownProjM2 = under.CrownProjM2;
  const double per = static_cast<double>(leavesPerCard > 0 ? leavesPerCard : 1) *
                     static_cast<double>(cards) * LocalArea_;
  if (per <= 0.0 || lai <= 0.0 || crownProjM2 <= 0.0) { return ScaleM_; }
  return static_cast<float>(std::sqrt(lai * crownProjM2 / per));
}

} // namespace outshine::Generators
