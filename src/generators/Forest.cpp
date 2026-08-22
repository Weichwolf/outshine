#include "Forest.h"

#include <numbers>
#include <cmath>

#include "Geodesy.h"
#include "Units.h"

namespace outshine::Generators {

namespace {

constexpr uint64_t kStreamsPerCell = 3;

float Unit24(uint64_t bits) { return (float)(bits & 0xFFFFFFu) * (1.0f / 16777216.0f); }
float Unit16(uint64_t bits) { return (float)(bits & 0xFFFFu) * (1.0f / 65536.0f); }

float SizeFactor(uint64_t bits, float sigma) {
  return 1.0f + sigma * (Unit16(bits) + Unit16(bits >> 16) - 1.0f) * 2.4494897f;
}

}

Forest::Forest(const Stem &stem, Span<const float> perM2ByRow, const AlpineLimit &limit)
    : Stem_(stem), PerM2_(perM2ByRow), Limit_(limit) {}

Span<const char *const> Forest::NoteNames() const noexcept {
  static const char *const kNames[kNotes] = {"noTemplate",  "zeroDensity", "densityDraw",
                                             "aboveTreeline", "tooSteep",  "woodyDraw",
                                             "highestStandAslM"};
  return Span<const char *const>(kNames, kNotes);
}

Forest::Lattice Forest::Of(const Region &region) {
  Lattice l;
  l.Cols = (int)(region.SpanEm() / kCellM + 0.5);
  l.Rows = (int)(region.SpanNm() / kCellM + 0.5);
  if (l.Cols < 1) l.Cols = 1;
  if (l.Rows < 1) l.Rows = 1;
  l.Em = region.SpanEm() / (double)l.Cols;
  l.Nm = region.SpanNm() / (double)l.Rows;
  return l;
}

Forest::Outcome Forest::Consider(const Ground &ground, const Lattice &lattice, Cell cell,
                                 Body *out) const noexcept {
  const Region &region = ground.Where();
  const uint64_t index = (uint64_t)cell.J * (uint64_t)lattice.Cols + (uint64_t)cell.I;
  const uint64_t place = region.Seed(index * kStreamsPerCell);
  const double eastM = ((double)cell.I + 0.25 + 0.5 * (double)Unit24(place)) * lattice.Em;
  const double northM = ((double)cell.J + 0.25 + 0.5 * (double)Unit24(place >> 24)) * lattice.Nm;

  int row = 0;
  if (!ground.CoverAt(eastM, northM).TryRow(&row) || (size_t)row >= PerM2_.Size())
    return Outcome::NoTemplate;
  const float perM2 = PerM2_[(size_t)row];
  if (perM2 <= 0.0f) return Outcome::ZeroDensity;

  const uint64_t draw = region.Seed(index * kStreamsPerCell + 1);
  if (Unit24(draw) > (float)((double)perM2 * lattice.Em * lattice.Nm)) return Outcome::DensityDraw;

  const double aslM = ground.HeightAslM(eastM, northM);

  const double latDeg = region.AnchorLat();
  const double jitterE = Wrap180(region.AnchorLon()) * kMPerDeg * std::cos(latDeg * kDeg2Rad);
  const double jitterN = latDeg * kMPerDeg;
  const double woody = Limit_.WoodyFraction(latDeg, aslM, jitterE + eastM, jitterN + northM);
  double steep = 0.0;
  if (woody > 0.0 && (size_t)row < ground.Table().Count())
    steep = Limit_.BareBySlope(ground.SlopeDeg(eastM, northM),
                               (double)ground.Table().At((size_t)row).SlopeMaxDeg);
  if (woody <= 0.0) return Outcome::AboveTreeline;
  if (steep >= 1.0) return Outcome::TooSteep;

  if ((double)Unit24(draw >> 24) >= woody * (1.0 - steep)) return Outcome::WoodyDraw;

  const float size = SizeFactor(region.Seed(index * kStreamsPerCell + 2), Stem_.HeightSigma);
  out->Em = eastM;
  out->Nm = northM;
  out->BaseAslM = aslM;
  out->RadiusM = Stem_.TrunkRadiusM * size;
  out->HeightM = (float)(Stem_.HeightM * (double)size);
  out->MassKg = Stem_.MassKg * size * size * size;
  out->YawRad = Unit16(place >> 48) * 2.0f * std::numbers::pi_v<float>;
  out->Contact = Stem_.Contact;
  return Outcome::Placed;
}

void Forest::Occupy(const Ground &ground, Yield &yield) const noexcept {
  const Lattice lattice = Of(ground.Where());
  for (int j = 0; j < lattice.Rows; j++) {
    for (int i = 0; i < lattice.Cols; i++) {
      Body body;
      const Outcome why = Consider(ground, lattice, Cell{i, j}, &body);
      switch (why) {
        case Outcome::NoTemplate: yield.Count(NoTemplate); continue;
        case Outcome::ZeroDensity: yield.Count(ZeroDensity); continue;
        case Outcome::DensityDraw: yield.Count(DensityDraw); continue;
        case Outcome::AboveTreeline: yield.Count(AboveTreeline); continue;
        case Outcome::TooSteep: yield.Count(TooSteep); continue;
        case Outcome::WoodyDraw: yield.Count(WoodyDraw); continue;
        case Outcome::Placed: break;
      }
      const Claim claim = yield.Place(body);

      if (claim.Why() == Claim::Outcome::Full) return;
      if (claim.Why() == Claim::Outcome::Placed) yield.Raise(HighestStandAslM, body.BaseAslM);
    }
  }
}

uint32_t Forest::Proposes(double areaM2) const noexcept {
  double densest = 0.0;
  for (size_t i = 0; i < PerM2_.Size(); i++)
    if ((double)PerM2_[i] > densest) densest = (double)PerM2_[i];
  const double cellM2 = kCellM * kCellM;
  double p = densest * cellM2;
  if (p > 1.0) p = 1.0;
  const double n = areaM2 / cellM2;
  const double mean = n * p;
  const double sd = std::sqrt(n * p * (1.0 - p));
  return (uint32_t)(mean + 8.0 * sd + 1.0);
}

bool Forest::At(const Ground &ground, double eastM, double northM, Body *out) const noexcept {
  const Lattice lattice = Of(ground.Where());
  const Cell cell{(int)std::floor(eastM / lattice.Em), (int)std::floor(northM / lattice.Nm)};
  if (cell.I < 0 || cell.J < 0 || cell.I >= lattice.Cols || cell.J >= lattice.Rows) return false;
  return Consider(ground, lattice, cell, out) == Outcome::Placed;
}

}
