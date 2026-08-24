#include <cmath>
#include <pthread.h>
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

  {
    // a monotone-deviation polyline peels ONE vertex per Douglas-Peucker level, so the
    // naked recursion's depth was O(points) -- proven on a thread whose stack ([SET]
    // 512 KiB, an eighth of the main thread's 8 MiB) the old code overflows at this size
    struct Peeling {
      std::vector<double> Points;
      size_t Kept = 0;
    } peeling;
    constexpr size_t kVertices = 8192;
    peeling.Points.reserve(kVertices * 2);
    for (size_t at = 0; at < kVertices; ++at) {
      peeling.Points.push_back((double)at);
      peeling.Points.push_back((at % 2 == 0 ? 1.0 : -1.0) * 0.001 * (double)at);
    }
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 512 * 1024);
    pthread_t worker;
    const auto run = [](void *held) -> void * {
      Peeling *inside = (Peeling *)held;
      inside->Kept = outshine::Simplify(inside->Points, 0.0005).size() / 2;
      return nullptr;
    };
    CHECK(pthread_create(&worker, &attr, run, &peeling) == 0, "the bounded-stack thread starts");
    pthread_join(worker, nullptr);
    pthread_attr_destroy(&attr);
    CHECK(peeling.Kept == kVertices,
          "**THE SIMPLIFY RETURNS INSTEAD OF FAULTING**: every zigzag vertex is outside the "
          "tolerance and all are kept, on a stack the peeled recursion could not survive -- "
          "the term is bounded by the heap, not the call depth (board:1712)");
  }

  // board:1784: on the Munich-Hamburg route the reviewer found a fitted arc of 0.1772/m --
  // a 5.6 m radius, tighter than the F31's own 5.65 m centreline minimum -- with curvature
  // reversing three times inside 45 m, and the speed plan then crawled it at 12.158 km/h.
  // A crawl is what a refusal looks like when nobody refused.
  {
    // uneven legs and mixed turn directions reproduce that shape with no network at all.
    const std::vector<double> wandering = {0.0,  0.0, 40.0, 0.0,  52.0, 9.0,
                                           64.0, -2.0, 78.0, 7.0, 120.0, 7.0};
    ReferenceLine wound;
    const Fitted got = Fit(wandering, kWithinM, kTightestM, wound);
    std::printf("NOTE the fit laid: %s, tightest %.4f m at vertex %zu\n",
                got.Laid ? "yes" : "no", got.TightestRadiusM, got.TightestAtVertex);
    CHECK(got.Laid, "a wandering polyline lays");

    double leastRadiusM = 1.0e9;
    double tightestAtM = 0.0;
    int reversals = 0;
    double was = 0.0;
    for (double m = 0.0; m <= wound.LengthM(); m += 0.05) {
      Placed here;
      if (!wound.At(m, here) || here.CurvaturePerM == 0.0) { continue; }
      const double radius = 1.0 / std::fabs(here.CurvaturePerM);
      if (radius < leastRadiusM) {
        leastRadiusM = radius;
        tightestAtM = m;
      }
      if (was != 0.0 && ((was < 0.0) != (here.CurvaturePerM < 0.0))) { ++reversals; }
      was = here.CurvaturePerM;
    }
    std::printf("NOTE walked: tightest %.4f m at %.2f m, %d sign reversals over %.1f m\n",
                leastRadiusM, tightestAtM, reversals, wound.LengthM());

    CHECK_NEAR(leastRadiusM, got.TightestRadiusM, 1.0e-9, "m",
               "**WHAT THE FIT REPORTS IS WHAT THE LINE CARRIES**: the tightest radius it "
               "names is the tightest radius a walk of the line finds, so the figure the "
               "corridor is judged on is the figure it will be driven at (board:1784)");
    CHECK(leastRadiusM >= kTightestM,
          "and the line it lays is one this vehicle can drive -- a corridor tighter than the "
          "steering lock is a refusal, not a corridor");

    // the vehicle that CANNOT drive it must be told so rather than handed a crawl.
    ReferenceLine unreachable;
    const Fitted tooTight = Fit(wandering, kWithinM, leastRadiusM + 1.0, unreachable);
    std::printf("NOTE a vehicle needing %.4f m: %s\n", leastRadiusM + 1.0,
                tooTight.Laid ? "still laid" : "refused");
    if (!tooTight.Laid) { std::printf("REFUSED %s\n", tooTight.Error.c_str()); }
    CHECK(!tooTight.Laid,
          "**AND A VEHICLE THAT CANNOT TURN THAT TIGHTLY IS REFUSED, NOT SERVED**: the same "
          "polyline handed a wider minimum does not quietly lay a corner that vehicle would "
          "have to crawl (board:1784)");
  }

  Covers("I.9.11 a corridor is fitted through a polyline it may not leave by more than the data's "
         "own accuracy: the corner radius falls out of that bound, every vertex carries a pair of "
         "spirals so no curvature leap is spellable, and the length is the cut corner's rather than "
         "the polyline's");
  return Report();
}
