/* WHAT THE ONE GEODESY COSTS, as a number instead of a sentence. Geodesy.h declares itself planar and
 * small-angle — "tens of nautical miles, not intercontinental" — and nothing in this tree said how
 * much that approximation is worth in metres at the acceptance place. It does now, and the same run
 * holds the body/ENU rotation pair to being an exact inverse, which is decidable with no reference at
 * all. */
#include "Check.h"
#include "Geodesy.h"

#include <cmath>

using namespace outshine;

namespace {

/* The acceptance place — Grohnde on the Weser, where the declared scenarios stand. */
constexpr double kRefLatDeg = 52.0294;
constexpr double kRefLonDeg = 9.4103;

/* The straight line through the ellipsoid between two surface points. Over the ranges below it is
 * shorter than the surface arc by d^3/(24 R^2) — 0.13 m at 50 km — which is two orders under the
 * planar model's own error, so it is the reference and the shortfall is stated rather than hidden. */
double EcefChordM(double latA, double lonA, double latB, double lonB) {
  double a[3], b[3];
  GeoToEcef(latA, lonA, 0.0, a);
  GeoToEcef(latB, lonB, 0.0, b);
  const double dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

/* Degrees of latitude / longitude that the PLANAR model believes are `m` metres. Feeding the model's
 * own inverse back through the exact ellipsoid is what isolates the model's error. */
double NorthDegForM(double m) { return m / kMPerDeg; }
double EastDegForM(double m) { return m / (kMPerDeg * std::cos(kRefLatDeg * kDeg2Rad)); }

double RelativeErrorAtM(double wantM) {
  const double north = EcefChordM(kRefLatDeg, kRefLonDeg, kRefLatDeg + NorthDegForM(wantM), kRefLonDeg);
  const double east = EcefChordM(kRefLatDeg, kRefLonDeg, kRefLatDeg, kRefLonDeg + EastDegForM(wantM));
  const double worst = std::fabs(north - wantM) > std::fabs(east - wantM) ? north : east;
  return (worst - wantM) / wantM;
}

} // namespace

int main() {
  Test::Covers("I.6 The one planar-ENU geodesy the simulator measures with (Geodesy.h)");

  /* DECIDABLE, no reference: EnuToBodyVec and BodyVecToEnu are declared exact inverses, and two
   * spellings of one rotation drifting apart is what that file exists to prevent. */
  const double kAttitudeDeg[4][3] = {
      {0.0, 0.0, 0.0}, {12.0, -7.0, 43.0}, {-95.0, 31.0, 187.0}, {179.0, -84.0, 359.0}};
  double worstResidualM = 0.0;
  for (const auto &att : kAttitudeDeg) {
    const double kBody[4][3] = {{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}, {3.0, -4.0, 12.0}};
    for (const auto &v : kBody) {
      double e = 0.0, n = 0.0, u = 0.0, f = 0.0, r = 0.0, d = 0.0;
      BodyVecToEnu(att[0], att[1], att[2], v[0], v[1], v[2], e, n, u);
      EnuToBodyVec(att[0], att[1], att[2], e, n, u, f, r, d);
      const double residual = std::sqrt((f - v[0]) * (f - v[0]) + (r - v[1]) * (r - v[1]) +
                                        (d - v[2]) * (d - v[2]));
      if (residual > worstResidualM) { worstResidualM = residual; }
    }
  }
  /* 1e-9 m over a 13 m vector is 7.7e-11 relative — two decimal digits above double's 2.2e-16 epsilon
   * times the ~30 trigonometric operations the round trip spends, so it is a tolerance on the
   * algorithm and not on the arithmetic. */
  CHECK_NEAR(worstResidualM, 0.0, 1e-9, "m", "body->ENU->body returns what it was given");
  Test::Note("worst body/ENU round-trip residual over 4 attitudes x 4 vectors", worstResidualM, "m");

  /* The planar model against the ellipsoid. The claim is the header's own scope, priced: at tens of
   * nautical miles the model is worth better than half a percent. */
  struct Range {
    const char *What;
    double M;
  };
  const Range kRange[4] = {{"planar model error at 1 km, worst of north and east", 1.0e3},
                           {"planar model error at 10 km, worst of north and east", 1.0e4},
                           {"planar model error at 50 km, worst of north and east", 5.0e4},
                           {"planar model error at 200 km, worst of north and east", 2.0e5}};
  for (const Range &range : kRange) {
    Test::Note(range.What, RelativeErrorAtM(range.M) * 100.0, "% of the range");
  }
  CHECK(std::fabs(RelativeErrorAtM(5.0e4)) < 5.0e-3,
        "at 50 km the planar model is within 0.5 % of the ellipsoid");
  Test::Note("50 km error in metres", RelativeErrorAtM(5.0e4) * 5.0e4, "m");

  /* The convention the file's own header states: reference first, and the offset is the second
   * point's FROM the first. A sign error here reverses every bearing in the simulator. */
  double eastM = 0.0, northM = 0.0;
  EnuOffsetM(kRefLatDeg, kRefLonDeg, kRefLatDeg + NorthDegForM(1000.0), kRefLonDeg, eastM, northM);
  CHECK_NEAR(northM, 1000.0, 1e-6, "m", "a point north of the reference is +north");
  CHECK_NEAR(eastM, 0.0, 1e-6, "m", "a point due north of the reference has no east offset");
  CHECK_NEAR(BearingDeg(kRefLatDeg, kRefLonDeg, kRefLatDeg, kRefLonDeg + EastDegForM(1000.0)), 90.0,
             1e-9, "deg", "due east of the reference bears 090");

  /* Longitude wrapping is part of the primitive, not something a caller remembers: the antimeridian
   * is 0.373 % of the surface away from being a special case that only shows up in a scenario. */
  double wrapEastM = 0.0, wrapNorthM = 0.0;
  EnuOffsetM(0.0, 179.999, 0.0, -179.999, wrapEastM, wrapNorthM);
  CHECK_NEAR(wrapEastM, 0.002 * kMPerDeg, 1e-6, "m",
             "an offset across the antimeridian is 0.002 deg east, not 359.998 west");

  return Test::Report();
}
