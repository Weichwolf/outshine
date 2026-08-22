#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"

#include "Wayfinding.h"

using outshine::Path::Plan;
using outshine::Path::Route;
using outshine::Path::Waypoint;

namespace {

constexpr double kMunichLat = 48.1371;
constexpr double kMunichLon = 11.5754;
constexpr double kHamburgLat = 53.5503;
constexpr double kHamburgLon = 9.9920;

constexpr double kStraightKm = 612.0;
constexpr double kShortestPlausibleKm = 700.0;
constexpr double kLongestPlausibleKm = 900.0;

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  Note("from Marienplatz, Munich", kMunichLat, "deg N");
  Note("and", kMunichLon, "deg E");
  Note("to Rathausmarkt, Hamburg", kHamburgLat, "deg N");
  Note("and", kHamburgLon, "deg E");
  Note("great-circle distance between them", kStraightKm, "km");

  const Waypoint from{kMunichLat, kMunichLon};
  const Waypoint to{kHamburgLat, kHamburgLon};
  const Route planned = Plan(from, to, 6371008.8);
  if (!planned.Found) { std::printf("REFUSED %s\n", planned.Error.c_str()); }

  CHECK(planned.Found,
        "**A ROUTE IS PLANNED FROM ONE COORDINATE TO ANOTHER OVER OSM'S OWN WAYS.** City centre to "
        "city centre, no waypoints given, no route stored -- two numbers in and a sequence of ways "
        "out. This is the first of the four things this case exercises, and none of the other three "
        "can be reached without it");
  if (!planned.Found) { return Report(); }

  Note("how far the route runs", planned.LengthM / 1000.0, "km");
  Note("how many legs it has", (double)planned.Legs.size(), "legs");
  CHECK(planned.LengthM / 1000.0 > kShortestPlausibleKm &&
            planned.LengthM / 1000.0 < kLongestPlausibleKm,
        "and its length is what a road between these two places is -- longer than the great circle "
        "because roads bend, and not so much longer that the search wandered");

  Covers("I.4.3 the car drives from Marienplatz in Munich to Rathausmarkt in Hamburg: a route "
         "planned over OSM's ways, a corridor built from them, the autopilot holding it and the "
         "declared vehicle's own physics carrying it -- arriving, without a crash, on roads that "
         "are correct enough to be driven");
  return Report();
}
