#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "Check.h"
#include "Geodesy.h"
#include "Wgs84.h"

namespace {

constexpr double kAgreesWithinM = 0.001;
constexpr double kSphereShownAsM = 1.0;
constexpr size_t kNearlyAntipodal = 3000;
constexpr double kAntipodalFromM = 19000000.0;

struct Row {
  double FromLatDeg = 0.0, FromLonDeg = 0.0;
  double ToLatDeg = 0.0, ToLonDeg = 0.0;
  double AlongM = 0.0;
};

[[nodiscard]] bool Rowed(const std::string &line, Row &out) {
  std::istringstream held(line);
  double az1 = 0.0, az2 = 0.0;
  return (held >> out.FromLatDeg >> out.FromLonDeg >> az1 >> out.ToLatDeg >> out.ToLonDeg >> az2 >>
          out.AlongM) &&
         out.AlongM > 0.0;
}

[[nodiscard]] double SphereApartM(const Row &one) {
  const double kR = outshine::Data::kWgs84A;
  const double fromLat = one.FromLatDeg * outshine::kDeg2Rad;
  const double toLat = one.ToLatDeg * outshine::kDeg2Rad;
  const double byLat = (one.ToLatDeg - one.FromLatDeg) * outshine::kDeg2Rad;
  double byLon = one.ToLonDeg - one.FromLonDeg;
  while (byLon > 180.0) { byLon -= 360.0; }
  while (byLon < -180.0) { byLon += 360.0; }
  byLon *= outshine::kDeg2Rad;
  const double half =
      std::sin(0.5 * byLat) * std::sin(0.5 * byLat) +
      std::cos(fromLat) * std::cos(toLat) * std::sin(0.5 * byLon) * std::sin(0.5 * byLon);
  return 2.0 * kR * std::asin(std::sqrt(half < 1.0 ? half : 1.0));
}

} // namespace

int main(int argc, char **argv) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const std::string under = argc > 1 ? argv[1] : std::string();
  CHECK(!under.empty(), "the runner was given the case directory it is to score");
  if (under.empty()) { return Report(); }
  std::printf("CASE %s\n", under.c_str());

  std::ifstream table(under + "/GeodTest-short.dat");
  CHECK(table.good(), "the case carries the table GeographicLib published");
  if (!table.good()) { return Report(); }

  std::vector<Row> rows;
  for (std::string line; std::getline(table, line);) {
    Row one;
    if (Rowed(line, one)) { rows.push_back(one); }
  }
  CHECK(rows.size() == 10000, "the short table holds the ten thousand rows it is published with");
  if (rows.empty()) { return Report(); }

  double worstM = 0.0, worstAtM = 0.0;
  double worstSphereM = 0.0, sphereAtM = 0.0;
  size_t settled = 0;
  double widestUnsettledM = 0.0, closestUnsettledM = 1.0e18;
  for (const Row &one : rows) {
    const outshine::Geodesic said = outshine::GeodesicOn(one.FromLatDeg,
                                                         one.FromLonDeg,
                                                         one.ToLatDeg,
                                                         one.ToLonDeg,
                                                         outshine::Data::kWgs84A,
                                                         outshine::Data::kWgs84F);
    if (!said.Converged) {
      if (one.AlongM > widestUnsettledM) { widestUnsettledM = one.AlongM; }
      if (one.AlongM < closestUnsettledM) { closestUnsettledM = one.AlongM; }
      continue;
    }
    ++settled;
    const double apartM = std::fabs(said.AlongM - one.AlongM);
    if (apartM > worstM) {
      worstM = apartM;
      worstAtM = one.AlongM;
    }
    const double sphereM = std::fabs(SphereApartM(one) - one.AlongM);
    if (sphereM > worstSphereM) {
      worstSphereM = sphereM;
      sphereAtM = one.AlongM;
    }
  }

  Note("geodesics scored", (double)rows.size(), "rows");
  Note("of them, the ones this method settles", (double)settled, "rows");
  Note("the closest pair it does NOT settle", closestUnsettledM / 1000.0, "km");
  Note("the worst disagreement of the geodesic", worstM * 1000.0, "mm");
  Note("where it was", worstAtM / 1000.0, "km");
  Note("the worst disagreement a SPHERE gives on the same table", worstSphereM / 1000.0, "km");
  Note("where that was", sphereAtM / 1000.0, "km");

  CHECK(settled + kNearlyAntipodal >= rows.size(),
        "the pairs this method cannot settle are the NEARLY ANTIPODAL ones and no others -- "
        "Vincenty's iteration is known not to converge there, and a method that quietly failed "
        "elsewhere would be hiding it among them");
  CHECK(closestUnsettledM > kAntipodalFromM,
        "and the closest pair it refuses is farther apart than any pair a scenario on one "
        "continent can declare, so what it cannot answer is named by its geometry rather than "
        "by a count");
  CHECK(worstM <= kAgreesWithinM,
        "**THE DISTANCE BETWEEN TWO COORDINATES IS THE GEODESIC**: GeographicLib computes it on "
        "the WGS84 ellipsoid in Maxima at 50 digits, far past what a double holds, so a "
        "disagreement is this engine's. One world space means one distance");
  CHECK(worstSphereM > kSphereShownAsM,
        "and a SPHERE is measurably not that geodesic on this same table -- the control that "
        "shows the test can tell the two apart, so a pass is not the tolerance being loose");

  Covers("one world space: the distance between two coordinates is the WGS84 geodesic, measured "
         "against a 50-digit reference this tree cannot influence");
  return Report();
}
