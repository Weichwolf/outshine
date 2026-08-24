#include <cmath>
#include <cstdint>
#include <cstdio>
#include <numbers>
#include <string>
#include <vector>

#include "Check.h"

#include "Wayfinding.h"

using outshine::Path::Network;
using outshine::Path::Route;
using outshine::Path::Waypoint;

namespace {

constexpr double kSnapM = 10.0;
constexpr double kIuggMeanRadiusM = 6371008.8;
constexpr double kLatDeg = 60.0;

void Lay(Network &net, double fromLon, double toLon) {
  const double points[4] = {kLatDeg, fromLon, kLatDeg, toLon};
  net.Lay(std::span<const double>(points, 2 * 2), 3.5, 0.1, 2);
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  // two ways END at the date line, 5.6 m apart across it -- inside the snap, so they are
  // ONE junction; an index that hashes raw longitude puts them half a billion columns
  // apart and the network weaves in pieces
  Network net(kSnapM, kIuggMeanRadiusM);
  Lay(net, 179.98, 179.99995);
  Lay(net, -179.99995, -179.98);
  std::string error;
  CHECK(net.Weave(error), "the two ways weave");
  CHECK(net.NodeCount() == 3,
        "**THE ANTIMERIDIAN'S TWO SIDES ARE NEIGHBOURS**: the ends 5.6 m apart across the "
        "line snap to ONE node -- four nodes would be a network in pieces (board:1719)");

  const Route across = net.Plan(Waypoint{kLatDeg, 179.981}, Waypoint{kLatDeg, -179.981}, 5.0);
  if (!across.Found) { std::printf("REFUSED %s\n", across.Error.c_str()); }
  CHECK(across.Found, "**AND A ROUTE CROSSES THE LINE** instead of refusing at the seam");
  Note("the crossing runs", across.LengthM, "m");
  CHECK(across.LengthM > 2000.0 && across.LengthM < 2500.0,
        "over ~2.2 km of road -- never the long way round the sphere, and never a "
        "360-degree pseudo-heading judged as a turn");
  // (TurnsRefused counts the U-turn reversals the search declines along the way; the
  // discriminator is Found itself -- an unwrapped lon delta reads the straight crossing
  // as a ~360-degree hairpin and refuses the route entirely)

  {
    // the adversarial fraction near the pole: two ends 49.7 m apart across a ROW    // boundary at 89.999N, the equatorward end placed 0.99 columns before its cell edge
    // -- one snap of longitude crosses TWO of the narrower equatorward columns, and a
    // one-column cross-row span missed the weld (board:1730)
    const double latCell = kSnapM / (kIuggMeanRadiusM * std::numbers::pi / 180.0);
    const int64_t rowP = (int64_t)std::floor(89.999 / latCell);
    const double boundary = (double)rowP * latCell;
    const double latA = boundary + 0.05 * latCell;
    const double latB = boundary - 0.05 * latCell;
    const double rowLatE = ((double)rowP - 0.5) * latCell;
    const double mPerDeg = kIuggMeanRadiusM * std::numbers::pi / 180.0;
    const double lonCellE = kSnapM / (mPerDeg * std::cos(rowLatE * std::numbers::pi / 180.0));
    const double lonA = (std::floor((0.0 + 180.0) / lonCellE) + 0.99) * lonCellE - 180.0;
    // the step east is 0.98 snaps AT THE PAIR'S OWN latitude (the boundary), so the
    // great-circle gap stays under the snap while the equatorward row counts it as
    // more than one of its narrower columns
    const double lonB =
        lonA + 0.98 * kSnapM / (mPerDeg * std::cos(boundary * std::numbers::pi / 180.0));

    Network polar(kSnapM, kIuggMeanRadiusM);
    // the equatorward end lays FIRST, so the poleward end does the searching -- the
    // direction where the neighbour's columns are the narrower ones
    const double first[4] = {latB, lonB - 30.0, latB, lonB};
    const double second[4] = {latA, lonA, latA, lonA + 30.0};
    polar.Lay(std::span<const double>(first, 2 * 2), 3.5, 0.1, 2);
    polar.Lay(std::span<const double>(second, 2 * 2), 3.5, 0.1, 2);
    CHECK(polar.Weave(error), "the polar pair weaves");
    Note("nodes at the adversarial fraction", (double)polar.NodeCount(), "nodes");
    CHECK(polar.NodeCount() == 3,
          "**THE CROSS-ROW SPAN CARRIES THE COSINE RATIO**: the two ends 49.7 m apart "
          "across the row boundary weld to one node -- the one-column span read this "
          "network as pieces (board:1730)");
  }

  Covers("I.9.13 the cell index lives on the sphere it indexes: a row's own latitude fixes "
         "the column modulus, the column wraps at the row's circumference, and a way "
         "meeting its neighbour across the antimeridian weaves and routes (board:1719, "
         "the route class 1524 names)");
  return Report();
}
