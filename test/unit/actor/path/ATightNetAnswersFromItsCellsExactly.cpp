#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"

#include "Wayfinding.h"

using outshine::Path::ApartM;
using outshine::Path::Network;
using outshine::Path::Waypoint;

namespace {

constexpr double kSnapM = 50.0;
constexpr double kIuggMeanRadiusM = 6371008.8;
constexpr int kGrid = 20;
constexpr double kSpacingM = 100.0;

[[nodiscard]] double LatOf(double lat0, double northM) {
  return lat0 + northM / (kIuggMeanRadiusM * 3.14159265358979323846 / 180.0);
}
[[nodiscard]] double LonOf(double lat0, double eastM) {
  return eastM / (kIuggMeanRadiusM * 3.14159265358979323846 / 180.0 *
                  std::cos(lat0 * 3.14159265358979323846 / 180.0));
}

// a 20x20 grid of 100 m spacing laid as row-ways: enough occupied cells that the reach
// box is SMALLER than the cell count and the walk takes the indexed arm, not the scan
void Grid(Network &net, double lat0) {
  for (int row = 0; row < kGrid; ++row) {
    std::vector<double> points;
    for (int column = 0; column < kGrid; ++column) {
      points.push_back(LatOf(lat0, row * kSpacingM));
      points.push_back(LonOf(lat0, column * kSpacingM));
    }
    net.Lay(points, 3.5, 0.1, 2);
  }
}

[[nodiscard]] size_t Distinct(std::vector<size_t> nodes) {
  std::sort(nodes.begin(), nodes.end());
  return (size_t)(std::unique(nodes.begin(), nodes.end()) - nodes.begin());
}

void Proves(double lat0, const char *where) {
  using namespace outshine::Test;
  Network net(kSnapM, kIuggMeanRadiusM);
  Grid(net, lat0);
  std::string error;
  const bool woven = net.Weave(error);
  if (!woven) { std::printf("REFUSED %s\n", error.c_str()); }
  CHECK(woven, "the grid weaves");
  CHECK(net.NodeCount() == kGrid * kGrid,
        "four hundred nodes stand -- one per grid point, none merged, none doubled");
  const Waypoint centre{LatOf(lat0, 10.0 * kSpacingM), LonOf(lat0, 10.0 * kSpacingM)};

  std::vector<size_t> held;
  net.Within(centre, 120.0, held);
  std::printf("NOTE %s: within 120 m answers %zu\n", where, held.size());
  CHECK(held.size() == 5 && Distinct(held) == 5,
        "**THE BOX WALK ANSWERS EXACTLY AND ONCE**: the centre and its four 100 m "
        "neighbours, no duplicate from column collapse, no miss at the rim (board:1720)");
  net.Within(centre, 145.0, held);
  CHECK(held.size() == 9 && Distinct(held) == 9,
        "and 145 m adds the four 141.4 m diagonals -- still each node once");
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  Proves(48.0, "at 48N");
  // poleward the parallels narrow: a query row's stride once collapsed onto one column
  // twice and the walk answered duplicates -- the per-row modulus holds at 85N too
  Proves(85.0, "at 85N");

  Covers("I.9.14 the woven index answers a tight net from its cells: the box walk "
         "enumerates (row, column) keys once each, per-row modulus, exact and "
         "duplicate-free at 48N and 85N (board:1720)");
  return Report();
}
