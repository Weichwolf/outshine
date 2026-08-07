#include "TreeFoliage.h"

#include <cmath>

#include "TreeRandom.h"

namespace outshine::World {

namespace {

constexpr float kTau = 6.2831853f;
/* The golden angle, the same phyllotaxis the grower rolls its laterals by: N laminae on one shoot
 * point that fan by 2pi/N alone would stack into a rosette from every direction but one. */
constexpr float kGolden = 2.39996323f;

} // namespace

void TreeFoliage::Build(const TreeMesh &mesh, const TreeSpecies::Leaf &leaf, int mult) {
  Inst_.clear();
  AreaM2_ = 0.0;
  const float len = leaf.Length > 1.0e-4f ? leaf.Length : 1.0f;
  ScaleM_ = leaf.CardH > 0.0f ? leaf.CardH / len : 0.1f;

  const int perPoint = (leaf.CardsPerPoint > 0 ? leaf.CardsPerPoint : 1) * (mult > 0 ? mult : 1);
  const size_t total = mesh.LeafPoints.size() * (size_t)perPoint;
  if (total == 0) { return; }
  Inst_.reserve(total * kFloats);

  TreeRandom rng(0x1eaf0001u);
  for (const TreeMesh::LeafPoint &p : mesh.LeafPoints) {
    for (int k = 0; k < perPoint; ++k) {
      const float roll = kGolden * (float)Inst_.size() / (float)kFloats + rng.Signed() * 0.35f;
      Inst_.insert(Inst_.end(), {p.Pos.X, p.Pos.Y, p.Pos.Z, std::fmod(roll, kTau),
                                 p.Dir.X, p.Dir.Y, p.Dir.Z, 0.0f});
    }
  }

  double lamina = 0.0;
  for (size_t i = 0; i + 2 < mesh.LeafIdx.size(); i += 3) {
    const float *a = &mesh.LeafVerts[(size_t)mesh.LeafIdx[i] * TreeMesh::kLeafFloats];
    const float *b = &mesh.LeafVerts[(size_t)mesh.LeafIdx[i + 1] * TreeMesh::kLeafFloats];
    const float *c = &mesh.LeafVerts[(size_t)mesh.LeafIdx[i + 2] * TreeMesh::kLeafFloats];
    const double e0[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    const double e1[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
    const double cx = e0[1] * e1[2] - e0[2] * e1[1];
    const double cy = e0[2] * e1[0] - e0[0] * e1[2];
    const double cz = e0[0] * e1[1] - e0[1] * e1[0];
    lamina += 0.5 * std::sqrt(cx * cx + cy * cy + cz * cz);
  }
  AreaM2_ = lamina * (double)ScaleM_ * (double)ScaleM_ * (double)Count();
}

} // namespace outshine::World
