#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "Check.h"
#include "Fit.h"
#include "ReferenceLine.h"

namespace {

// A JUNCTION IS NOT A ROAD, AND A CORNER IS BOUNDED BY THE JUNCTION.
//
// The accuracy bound says how far a built alignment may leave the polyline it is fitted through.
// Along a road that is the road's own half width: the built carriageway must stay on the
// carriageway. AT A CORNER it is not, because the corner is a junction, and a junction is the
// area where two carriageways overlap -- wider than either.
//
// The KERB is the bound and it is constructible. Two centrelines of half widths wA and wB meet at
// a deflection D; offset each inward by its own half width and the two offsets cross at
//
//   E = sqrt(wA^2 + wB^2 - 2 wA wB cos D) / sin D
//
// which for equal widths is w / cos(D/2) -- the distance from the intersection point to the inner
// kerb, and the furthest inside the junction any wheel can be while still on made ground. It
// takes unequal widths without a second rule and it degenerates correctly at both ends: at
// D -> 0 it grows without bound, because two nearly parallel roads overlap forever, and at
// D -> pi it falls to w.
//
// WHY IT DECIDES WHETHER A CAR CAN DRIVE. A wide arc at a sharp corner cuts FAR from the corner
// point, and the accuracy bound therefore caps the radius:
//
//   byAccuracy = withinM / (ShiftShare(D)/cos(D/2) - 1)
//
// At D = 1.9147 rad -- 109.7 degrees, an ordinary city corner -- one road's half width of 3.75 m
// gives 4.68 m, and the F31 bends to 4.90 m. **It refused by 0.23 m.** The junction of two 7.5 m
// roads gives 6.51 m and a radius of 8.13 m, which is drivable. Measured on the shipped network,
// two of five sampled destinations routed and then failed to lay a corridor at exactly this
// corner; with the kerb bound all five lay.
//
// AND THE CASE MUST REFUSE WHERE REFUSING IS RIGHT. Two 3 m alleys meeting at the same angle give
// a kerb of 2.60 m and a radius of 3.25 m, which this car cannot bend to. A bound that let
// everything through would satisfy the first half and be a licence rather than a rule.
constexpr double kDeflectionRad = 1.9147;
constexpr double kLegM = 50.0;
constexpr double kLockM = 4.901673;

[[nodiscard]] std::vector<double> Corner(void) {
  std::vector<double> points;
  points.push_back(0.0);
  points.push_back(0.0);
  points.push_back(kLegM);
  points.push_back(0.0);
  points.push_back(kLegM + kLegM * std::cos(kDeflectionRad));
  points.push_back(kLegM * std::sin(kDeflectionRad));
  return points;
}

[[nodiscard]] double KerbM(double halfM) {
  return std::sqrt(2.0 * halfM * halfM * (1.0 - std::cos(kDeflectionRad))) /
         std::sin(kDeflectionRad);
}

struct Laid {
  bool Stood = false;
  double RadiusM = 0.0;
  std::string Said;
};

[[nodiscard]] Laid Over(double halfM, bool byTheJunction) {
  const std::vector<double> points = Corner();
  std::vector<double> withinAtM(points.size() / 2, halfM);
  if (byTheJunction) { withinAtM[1] = KerbM(halfM); }
  outshine::ReferenceLine line;
  const outshine::Fitted fitted =
      outshine::Fit(points, halfM, kLockM, {}, line, withinAtM);
  return Laid{fitted.Laid, fitted.TightestRadiusM, fitted.Error};
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const Laid wide = Over(3.75, true);
  const Laid narrow = Over(1.5, true);
  const Laid byTheRoad = Over(3.75, false);

  std::printf("  two 7.5 m roads, junction bound %5.2f m   %s  radius %6.3f m\n", KerbM(3.75),
              wide.Stood ? "LAID   " : "REFUSED", wide.RadiusM);
  std::printf("  two 3.0 m alleys, junction bound %5.2f m   %s  radius %6.3f m\n", KerbM(1.5),
              narrow.Stood ? "LAID   " : "REFUSED", narrow.RadiusM);
  std::printf("  two 7.5 m roads, ONE road's half width     %s  radius %6.3f m\n",
              byTheRoad.Stood ? "LAID   " : "REFUSED", byTheRoad.RadiusM);
  std::printf("  the vehicle bends to %.3f m\n", kLockM);
  if (!narrow.Stood) { std::printf("  the alley says: %s\n", narrow.Said.c_str()); }

  CHECK(wide.Stood && wide.RadiusM >= kLockM,
        "**A CORNER IS BOUNDED BY ITS JUNCTION**: two 7.5 m carriageways meeting at 109.7 degrees "
        "overlap in an area 6.51 m across at its inner kerb, and an arc that may use it fits at a "
        "radius this car can bend to. Bounded by ONE road's half width the same corner caps at "
        "4.68 m against a 4.90 m lock and refuses -- at an ordinary city corner");

  CHECK(!byTheRoad.Stood,
        "and the control is the bound itself: the identical corner judged by one road's half "
        "width REFUSES, so what the check above measures is the junction and not the geometry");

  CHECK(!narrow.Stood,
        "and a junction that is genuinely too tight still refuses BY NAME: two 3 m alleys give a "
        "kerb of 2.60 m and an arc of 3.25 m, which this car cannot bend to. A bound that let "
        "everything through would pass the first check and be a licence rather than a rule");

  Covers("a corner is bounded by the junction its two carriageways form, whose inner kerb lies "
         "sqrt(wA^2 + wB^2 - 2 wA wB cos D)/sin D from the intersection, so an ordinary city "
         "corner fits at a radius the declared vehicle can drive");
  return Report();
}
