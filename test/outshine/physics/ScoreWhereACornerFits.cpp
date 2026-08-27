#include <cmath>
#include <numbers>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "Check.h"
#include "Alignment.h"
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
// takes unequal widths without a second rule, and its two ends are worth reading off the closed
// form rather than guessed at. D is the DEFLECTION, so a straight road is D = 0 and a hairpin is
// D -> pi:
//
//   D -> 0     cos(D/2) -> 1   ->  E -> w            a corner that is barely a corner is a road
//   D -> pi    cos(D/2) -> 0   ->  E grows unbounded  two nearly reversed roads overlap forever
//
//   D = 109.7 deg   E =   6.51 m        D = 170 deg   E =  43.03 m
//   D =  57.3 deg   E =   4.27 m        D = 178 deg   E = 214.87 m
//
// **The unbounded end is a defect and not a licence.** An accuracy bound of 215 m means the built
// road bears no relation to the declared one: `byAccuracy` goes effectively infinite and the
// radius is then set by the tangent room alone, ignoring every vertex the arc is meant to follow.
// So the kerb is capped by the polyline that carries it -- an arc may not leave a vertex by more
// than the shorter of the two legs meeting there, because past that it is no longer following
// the road but replacing it.
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
  return outshine::JunctionKerbM(halfM, halfM, kDeflectionRad, 0.0);
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

  {
    const double hairpinRad = 178.0 * std::numbers::pi / 180.0;
    const double loose = outshine::JunctionKerbM(3.75, 3.75, hairpinRad, 0.0);
    const double held = outshine::JunctionKerbM(3.75, 3.75, hairpinRad, kLegM);
    const double gentle = outshine::JunctionKerbM(3.75, 3.75, 0.1, kLegM);
    std::printf("  a 178 deg hairpin, uncapped %8.2f m   capped by a %.0f m leg %8.2f m\n", loose,
                kLegM, held);
    std::printf("  a 5.7 deg bend                                          %8.2f m\n", gentle);

    CHECK(loose > 200.0,
          "the closed form really does diverge at the hairpin end: two 7.5 m roads deflected 178 "
          "degrees have inner kerbs that meet 214.87 m from the apex, which is geometry and not "
          "an error -- so the case is capping something real");

    CHECK(held <= kLegM + 1.0e-9 && held < loose,
          "**AND AN UNBOUNDED KERB IS A DEFECT, NOT A LICENCE**: a 215 m accuracy bound makes "
          "byAccuracy effectively infinite, so the radius falls to the tangent room alone and the "
          "built road stops following the vertices it is fitted through. The kerb is capped by "
          "the shorter leg meeting the corner, because past that an arc is replacing the road "
          "rather than following it");

    CHECK(std::fabs(gentle - 3.75) < 0.01,
          "and the other end reads as the closed form says: at a 5.7 degree bend cos(D/2) is very "
          "nearly one and the kerb is the half width itself -- a corner that is barely a corner "
          "is a road. Both ends were written backwards in this file's own derivation until the "
          "numbers were printed");
  }

  Covers("a corner is bounded by the junction its two carriageways form, whose inner kerb lies "
         "sqrt(wA^2 + wB^2 - 2 wA wB cos D)/sin D from the intersection, so an ordinary city "
         "corner fits at a radius the declared vehicle can drive");
  return Report();
}
