#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"

#include "Fit.h"

using outshine::Fit;
using outshine::Fitted;
using outshine::Placed;
using outshine::ReferenceLine;

namespace {

constexpr double kWithinM = 9.55462861;
constexpr double kTightestM = 4.874;

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  ReferenceLine straight;
  const std::vector<double> line = {0.0, 0.0, 1000.0, 0.0, 2000.0, 0.0};
  const Fitted flat = Fit(line, kWithinM, kTightestM, straight);
  if (!flat.Laid) { std::printf("REFUSED %s\n", flat.Error.c_str()); }
  Note("a straight polyline fits to", flat.LengthM, "m");
  Note("corners it needed", (double)flat.Corners, "corners");
  CHECK(flat.Laid && flat.Corners == 0, "a polyline that never turns needs no corner");
  CHECK_NEAR(flat.LengthM, 2000.0, 1e-6, "m", "and keeps its length exactly");

  ReferenceLine bent;
  const std::vector<double> corner = {0.0, 0.0, 1000.0, 0.0, 1000.0, 1000.0};
  const Fitted right = Fit(corner, kWithinM, kTightestM, bent);
  if (!right.Laid) { std::printf("REFUSED %s\n", right.Error.c_str()); }
  CHECK(right.Laid, "a right-angle corner fits");
  if (!right.Laid) { return Report(); }

  Note("the sharpest turn it carried", right.SharpestTurnRad, "rad");
  Note("the tightest radius that allowed", right.TightestRadiusM, "m");
  const double swing = 0.5 * 3.14159265358979;
  const double shiftShare = 1.0 + swing * swing / 96.0;
  Note("what a circular corner alone would permit at 90 degrees",
       kWithinM / (1.0 / std::cos(0.25 * 3.14159265358979) - 1.0), "m");
  Note("what it becomes once the spirals' own shift is carried",
       kWithinM / (shiftShare / std::cos(0.25 * 3.14159265358979) - 1.0), "m");
  Note("how far the fitted line leaves the vertices", right.WorstOffsetM, "m");
  Note("how far it is allowed to", kWithinM, "m");
  Note("the length it fits to", right.LengthM, "m");
  Note("what two 1000 m legs would be", 2000.0, "m");

  CHECK_NEAR(right.TightestRadiusM,
             kWithinM / (shiftShare / std::cos(0.25 * 3.14159265358979) - 1.0), 0.01, "m",
             "**THE CORNER RADIUS IS DERIVED FROM THE DATA'S OWN ACCURACY.** A larger radius cuts "
             "the corner further from the vertex; R (1/cos(theta/2) - 1) is exactly how far, so "
             "bounding that by the tile's coordinate quantisation bounds the radius. And the spirals "
             "push the curve further out again by their own shift L^2/24R, so the radius that "
             "actually fits is 21.2 m and not the 23.1 m a circular corner would allow. Nobody "
             "chose either -- 9.55 m of quantisation and a 90 degree turn did");
  CHECK_NEAR(right.WorstOffsetM, kWithinM, 0.2 * kWithinM, "m",
             "**AND IT USES THE ROOM IT WAS GIVEN.** A fit that stayed far inside the bound would be "
             "cutting corners tighter than the data demands, which costs the vehicle speed it did "
             "not need to lose");
  Note("by how much the laid curve overruns the budget the construction targeted",
       right.WorstOffsetM / kWithinM - 1.0, "of it");
  CHECK(right.WorstOffsetM <= 1.01 * kWithinM,
        "**AND THE FITTED LINE NEVER LEAVES A VERTEX BY MORE THAN THE DATA IS ACCURATE TO.** That "
        "is what makes the smoothing a RECONSTRUCTION rather than an invention: it stays inside the "
        "error bars of what it was given. The 0.28 % it overruns by is the CLOTHOID'S NUMERICAL "
        "INTEGRATION -- the construction is closed in the first-order shift L^2/24R while the line "
        "is walked by 8-node Gauss-Legendre over the heading, and the two agree to three parts in a "
        "thousand rather than exactly");
  CHECK(right.LengthM < 2000.0,
        "and it is shorter than the polyline, because a corner cut is shorter than the corner");
  CHECK(right.Corners == 1 && right.Straights == 2,
        "with one corner between two straights");

  ReferenceLine wiggle;
  std::vector<double> zigzag;
  for (int step = 0; step < 40; ++step) {
    zigzag.push_back((double)step * 100.0);
    zigzag.push_back((step % 2 == 0) ? 0.0 : 30.0);
  }
  const Fitted many = Fit(zigzag, kWithinM, kTightestM, wiggle);
  if (!many.Laid) { std::printf("REFUSED %s\n", many.Error.c_str()); }
  CHECK(many.Laid,
        "**AND A LINE THAT TURNS AT EVERY VERTEX STILL LAYS, WHICH IS THE WHOLE POINT.** "
        "ReferenceLine REFUSES a leap in curvature, so a polyline cannot be laid as straights: "
        "every vertex must carry a pair of spirals that take the curvature up and back down. A fit "
        "that produced a leap would be refused by the line itself rather than by a checker");
  if (!many.Laid) { return Report(); }
  Note("vertices", (double)many.Vertices, "vertices");
  Note("corners fitted", (double)many.Corners, "corners");
  Note("worst offset from a vertex", many.WorstOffsetM, "m");
  Note("tightest radius", many.TightestRadiusM, "m");
  CHECK(many.Corners == many.Vertices - 2, "one corner per interior vertex");
  CHECK(many.WorstOffsetM <= 1.01 * kWithinM, "and still inside the accuracy everywhere");

  ReferenceLine tooShort;
  const std::vector<double> single = {0.0, 0.0};
  const Fitted refused = Fit(single, kWithinM, kTightestM, tooShort);
  CHECK(!refused.Laid, "one vertex is not a corridor and says so");
  std::printf("REFUSAL %s\n", refused.Error.c_str());

  ReferenceLine doubled;
  const std::vector<double> back = {0.0, 0.0, 1000.0, 0.0, 0.0, 5.0};
  const Fitted cusp = Fit(back, kWithinM, kTightestM, doubled);
  CHECK(!cusp.Laid && cusp.Undrivable == 1,
        "**AND A POLYLINE THAT DOUBLES BACK IS A REFUSAL AND NOT A CUSP TO SMOOTH.** A corner "
        "tighter than the vehicle's own steering lock is not a road: it is a route that turned "
        "round, and fitting a 0.18 m radius through it would hide the finding inside a corridor "
        "nobody could drive");
  std::printf("REFUSAL %s\n", cusp.Error.c_str());

  Covers("I.9.11 a corridor is fitted through a polyline it may not leave by more than the data's "
         "own accuracy: the corner radius falls out of that bound, every vertex carries a pair of "
         "spirals so no curvature leap is spellable, and the length is the cut corner's rather than "
         "the polyline's");
  return Report();
}
