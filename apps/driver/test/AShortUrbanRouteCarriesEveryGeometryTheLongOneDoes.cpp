#include <cmath>
#include <cstdio>
#include <map>
#include <string>

#include "Check.h"

#include "CurlTransport.h"
#include <outshine/Assembled.h>
#include <outshine/Column.h>
#include <outshine/Store.h>

#include "Assembly.h"
#include "DriveAssembly.h"
#include "DeclaredSources.h"
#include "GroundStack.h"
#include "ScenarioRead.h"

using outshine::Sim::AssembleDrive;
using outshine::Sim::DriveProduct;
using outshine::Ground::GroundStack;
using outshine::Sink;

namespace {

// Munich's inner ring, Marienplatz to Nymphenburg: about 20 km that stays inside the city.
// The long route is 775 km of which the overwhelming majority is motorway -- the geometry class
// with the widest radii and the fewest class changes per kilometre. The reconstruction's hard
// cases are urban: tight radii, short segments, a class change every few hundred metres, and
// junctions dense enough that corners share their straights. This case is the regression gate
// for that geometry; the long one stays as the SCALE proof and is run by name (board:1795).
constexpr double kMarienplatzLat = 48.1371;
constexpr double kMarienplatzLon = 11.5754;
constexpr double kNymphenburgLat = 48.1583;
constexpr double kNymphenburgLon = 11.5033;
constexpr int kZoom = 14;

class Harness : public Sink {
public:
  void Number(const char *what, double value, const char *unit) override {
    outshine::Test::Note(what, value, unit);
    Seen[what] = value;
  }
  std::map<std::string, double> Seen;
  void Claim(bool held, const char *why) override {
    outshine::Test::Checked(held, "the short drive", why, __FILE__, __LINE__);
  }
  void Near(double got, double want, double within, const char *unit, const char *why) override {
    outshine::Test::CheckedNear(got, want, within, unit, why, "the short drive", __FILE__,
                                __LINE__);
  }
  void Say(const std::string &line) override { std::printf("%s\n", line.c_str()); }
};

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  Harness harness;
  GroundStack stack;
  DriveProduct drive;
  std::string scenarioText;
  {
    std::FILE *const file = std::fopen("apps/driver/src/f31.scenario", "rb");
    if (file != nullptr) {
      int one = 0;
      while ((one = std::fgetc(file)) != EOF) { scenarioText.push_back((char)one); }
      std::fclose(file);
    }
  }
  outshine::Scenario declared;
  std::string readError;
  if (!outshine::ReadScenario(scenarioText.data(), scenarioText.size(), declared, readError)) {
    std::printf("REFUSED %s\n", readError.c_str());
    return Report();
  }
  declared.Driven.Declared = true;
  declared.Driven.FromLatDeg = kMarienplatzLat;
  declared.Driven.FromLonDeg = kMarienplatzLon;
  declared.Driven.ToLatDeg = kNymphenburgLat;
  declared.Driven.ToLonDeg = kNymphenburgLon;
  declared.Driven.Zoom = kZoom;

  outshine::Store scene;
  outshine::Column<outshine::Vehicle> vehicles;
  outshine::Column<outshine::Drive> drives;
  outshine::Column<outshine::Traits> kinds;
  outshine::Assembled stood;
  if (!scene.Open(outshine::AssembledCapacity(declared)) || !vehicles.Open(scene) ||
      !drives.Open(scene) || !kinds.Open(scene) ||
      !outshine::Assemble(declared, scene, vehicles, drives, kinds, stood, readError)) {
    std::printf("REFUSED %s\n", readError.c_str());
    return Report();
  }

  outshine::Host::CurlTransport::Config wiring;
  outshine::Host::CurlTransport wire(wiring);
  const bool laid = AssembleDrive(
      scene, stood, vehicles, drives, declared.Ground, stack, wire,
      outshine::Sim::Provision{"/tmp/outshine-drive-cache", "src/assets",
                               {outshine::Data::ShippedProviders().begin(),
                                outshine::Data::ShippedProviders().end()}},
      harness, drive);
  CHECK(laid, "**A SHORT URBAN ROUTE LAYS** -- ways fetched live, a corridor fitted through "
              "them, the real ground under it, the F31 standing on it");
  if (!laid) { return Report(); }

  const double routeKm = drive.Way.Line.LengthM() / 1000.0;
  const auto &fit = drive.Shape;
  Note("the route this case checks", routeKm, "km");
  Note("corners the fit laid", (double)fit.Corners, "corners");
  Note("corners per kilometre", routeKm > 0.0 ? (double)fit.Corners / routeKm : 0.0, "per km");
  Note("the tightest radius it laid", fit.TightestRadiusM, "m");
  Note("runs of same-sign turns", (double)fit.Runs, "runs");
  Note("the longest such run", (double)fit.LongestRunVertices, "vertices");
  Note("corners under their road class's minimum", (double)fit.UnderClass, "corners");
  Note("the sharpest turn it carried", fit.SharpestTurnRad * 180.0 / std::numbers::pi, "deg");
  Note("turns past a right angle", (double)fit.TurnsPastRightAngle, "turns");
  Note("how far the fit leaves the vertices", fit.WorstOffsetM, "m");

  CHECK(routeKm > 3.0 && routeKm < 30.0,
        "**AND IT IS SHORT ENOUGH TO RUN EVERY ROUND** -- a route inside one city, not a "
        "country crossing, so the geometry that is hard is exercised at a cost the gate can "
        "carry (board:1795)");
  CHECK(fit.Corners > 0 && (double)fit.Corners / routeKm > 2.0,
        "**AND URBAN GEOMETRY IS WHAT IT CARRIES**: a motorway route offers a handful of bends "
        "per kilometre and the widest radii in the class table -- an inner-city route offers "
        "the tight radii, the short segments and the class changes the reconstruction is hard "
        "at, which is why THIS is the regression gate and the long route is the scale proof");

  {
    const auto &plan = drive.Way.Profile;
    const size_t crawling = plan.StationsUnder(30.0 / 3.6);
    const double crawlShare =
        plan.SampleCount() > 0 ? (double)crawling / (double)plan.SampleCount() : 0.0;
    Note("stations the plan holds", (double)plan.SampleCount(), "stations");
    Note("the speed it plans at p01", plan.Quantile(0.01) * 3.6, "km/h");
    Note("at p50", plan.Quantile(0.50) * 3.6, "km/h");
    Note("stations planned under 30 km/h", (double)crawling, "stations");
    Note("what share of the route that is", crawlShare * 100.0, "%");

    const outshine::SpeedProfile::Standing road = plan.SlowestBound();
    std::printf("NOTE the slowest station the road holds = %.3f km/h at %.3f km by '%s'\n",
                road.Ms * 3.6, road.AtM / 1000.0, outshine::SpeedProfile::NameOf(road.By));
    CHECK(plan.SampleCount() > 0, "and the plan holds stations to be asked about");
  }

  Covers("I.4.13 a short urban route carries the geometry the reconstruction is hard at -- "
         "tight radii, short segments, a class change every few hundred metres -- at a cost the "
         "gate can pay every round, so the country crossing is the scale proof and not the "
         "regression gate (board:1795)");
  return Report();
}
