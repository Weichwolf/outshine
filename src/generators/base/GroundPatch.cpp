#include "GroundPatch.h"

#include <cmath>

#include "Units.h"

namespace outshine::Generators {

namespace {

double Clamped(double v, double lo, double hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

}

std::shared_ptr<const GroundPatch>
GroundPatch::Complete(const Tile &region, int side, Span<const Posting> postings) {
  if (side < 2 || postings.Size() != (size_t)side * (size_t)side) { return nullptr; }
  for (const Posting &p : postings) {
    if (p.Height.Where() != GroundSample::State::Resolved) { return nullptr; }
  }
  const double steps = (double)(side - 1);
  return std::shared_ptr<const GroundPatch>(
      new GroundPatch(side, region.SpanEm() / steps, region.SpanNm() / steps, postings));
}

GroundPatch::GroundPatch(int side, double spacingEm, double spacingNm, Span<const Posting> postings)
    : Side_(side), SpacingEm_(spacingEm), SpacingNm_(spacingNm) {
  AslM_.reserve(postings.Size());
  for (const Posting &p : postings) {
    double aslM = 0.0;
    (void)p.Height.TryAslM(&aslM);
    AslM_.push_back(aslM);
  }
}

double GroundPatch::HeightAslM(double eastM, double northM) const noexcept {
  const double u = Clamped(eastM / SpacingEm_, 0.0, (double)(Side_ - 1));
  const double v = Clamped(northM / SpacingNm_, 0.0, (double)(Side_ - 1));
  const int i0 = (int)u, j0 = (int)v;
  const int i1 = i0 + 1 < Side_ ? i0 + 1 : i0, j1 = j0 + 1 < Side_ ? j0 + 1 : j0;
  const double fu = u - (double)i0, fv = v - (double)j0;
  const double a = AslM_[(size_t)j0 * (size_t)Side_ + (size_t)i0];
  const double b = AslM_[(size_t)j0 * (size_t)Side_ + (size_t)i1];
  const double c = AslM_[(size_t)j1 * (size_t)Side_ + (size_t)i0];
  const double d = AslM_[(size_t)j1 * (size_t)Side_ + (size_t)i1];
  return (a + (b - a) * fu) * (1.0 - fv) + (c + (d - c) * fu) * fv;
}

void GroundPatch::GradientAt(double eastM,
                             double northM,
                             double *dhde,
                             double *dhdn) const noexcept {
  *dhde = (HeightAslM(eastM + SpacingEm_, northM) - HeightAslM(eastM - SpacingEm_, northM)) /
          (2.0 * SpacingEm_);
  *dhdn = (HeightAslM(eastM, northM + SpacingNm_) - HeightAslM(eastM, northM - SpacingNm_)) /
          (2.0 * SpacingNm_);
}

double GroundPatch::SlopeDeg(double eastM, double northM) const noexcept {
  double de = 0.0, dn = 0.0;
  GradientAt(eastM, northM, &de, &dn);
  return std::atan(std::sqrt(de * de + dn * dn)) * kRad2Deg;
}

size_t GroundPatch::HeapBytes() const {
  return AslM_.capacity() * sizeof(double);
}

}
