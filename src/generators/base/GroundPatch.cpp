#include <span>
#include "GroundPatch.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <cstddef>

#include "math/Units.h"

namespace outshine::Generators {

namespace {

double Clamped(double v, double lo, double hi) {
  return std::clamp(v, lo, hi);
}

} // namespace

std::shared_ptr<const GroundPatch>
GroundPatch::Complete(const Tile &region, int side, std::span<const Posting> postings) {
  if (side < 2 || postings.size() != static_cast<size_t>(side) * static_cast<size_t>(side)) {
    return nullptr;
  }
  for (const Posting &p : postings) {
    if (p.Height.Where() != GroundSample::State::Resolved) { return nullptr; }
  }
  const auto steps = static_cast<double>(side - 1);
  return std::shared_ptr<const GroundPatch>(
      new GroundPatch(side, region.SpanEm() / steps, region.SpanNm() / steps, postings));
}

GroundPatch::GroundPatch(int side,
                         double spacingEm,
                         double spacingNm,
                         std::span<const Posting> postings)
    : Side_(side), SpacingEm_(spacingEm), SpacingNm_(spacingNm) {
  AslM_.reserve(postings.size());
  for (const Posting &p : postings) {
    double aslM = 0.0;
    aslM = p.Height.AslM().value_or(aslM);
    AslM_.push_back(aslM);
  }
}

double GroundPatch::HeightAslM(EastNorth at) const noexcept {
  const double u = Clamped(at.EastM / SpacingEm_, 0.0, static_cast<double>(Side_ - 1));
  const double v = Clamped(at.NorthM / SpacingNm_, 0.0, static_cast<double>(Side_ - 1));
  const int i0 = static_cast<int>(u);
  const int j0 = static_cast<int>(v);
  const int i1 = i0 + 1 < Side_ ? i0 + 1 : i0;
  const int j1 = j0 + 1 < Side_ ? j0 + 1 : j0;
  const double fu = u - static_cast<double>(i0);
  const double fv = v - static_cast<double>(j0);
  const double a =
      AslM_[static_cast<size_t>(j0) * static_cast<size_t>(Side_) + static_cast<size_t>(i0)];
  const double b =
      AslM_[static_cast<size_t>(j0) * static_cast<size_t>(Side_) + static_cast<size_t>(i1)];
  const double c =
      AslM_[static_cast<size_t>(j1) * static_cast<size_t>(Side_) + static_cast<size_t>(i0)];
  const double d =
      AslM_[static_cast<size_t>(j1) * static_cast<size_t>(Side_) + static_cast<size_t>(i1)];
  return (a + (b - a) * fu) * (1.0 - fv) + (c + (d - c) * fu) * fv;
}

Gradient GroundPatch::GradientAt(EastNorth at) const noexcept {
  const EastNorth eastOf = {.EastM = at.EastM + SpacingEm_, .NorthM = at.NorthM};
  const EastNorth westOf = {.EastM = at.EastM - SpacingEm_, .NorthM = at.NorthM};
  const EastNorth northOf = {.EastM = at.EastM, .NorthM = at.NorthM + SpacingNm_};
  const EastNorth southOf = {.EastM = at.EastM, .NorthM = at.NorthM - SpacingNm_};
  return {.PerEastM = (HeightAslM(eastOf) - HeightAslM(westOf)) / (2.0 * SpacingEm_),
          .PerNorthM = (HeightAslM(northOf) - HeightAslM(southOf)) / (2.0 * SpacingNm_)};
}

double GroundPatch::SlopeDeg(EastNorth at) const noexcept {
  const Gradient rise = GradientAt(at);
  return std::atan(std::sqrt(rise.PerEastM * rise.PerEastM + rise.PerNorthM * rise.PerNorthM)) *
         kRad2Deg;
}

size_t GroundPatch::HeapBytes() const {
  return AslM_.capacity() * sizeof(double);
}

} // namespace outshine::Generators
