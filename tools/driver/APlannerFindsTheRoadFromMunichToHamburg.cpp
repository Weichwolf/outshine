#include <chrono>
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
using outshine::Sim::Ridden;
using outshine::Sink;

namespace {

constexpr double kMarienplatzLat = 48.1371;
constexpr double kMarienplatzLon = 11.5754;
constexpr double kRathausmarktLat = 53.5503;
constexpr double kRathausmarktLon = 9.9920;
constexpr int kZoom = 10;
constexpr double kStepS = 1.0e-3;
constexpr long kMostSteps = 40000000;

class Harness : public Sink {
public:
  void Number(const char *what, double value, const char *unit) override {
    outshine::Test::Note(what, value, unit);
    Seen[what] = value;
  }
  std::map<std::string, double> Seen;
  void Claim(bool held, const char *why) override {
    outshine::Test::Checked(held, "the journey", why, __FILE__, __LINE__);
  }
  void Near(double got, double want, double within, const char *unit, const char *why) override {
    outshine::Test::CheckedNear(got, want, within, unit, why, "the journey", __FILE__, __LINE__);
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
    std::FILE *const file = std::fopen("tools/driver/f31.scenario", "rb");
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
  declared.Driven.ToLatDeg = kRathausmarktLat;
  declared.Driven.ToLonDeg = kRathausmarktLon;
  declared.Driven.Zoom = kZoom;
  outshine::Store scene;
  outshine::Column<outshine::Vehicle> vehicles;
  outshine::Column<outshine::Drive> drives;
  outshine::Column<outshine::Traits> kinds;
  outshine::Assembled stood;
  if (!scene.Open(outshine::AssembledCapacity(declared)) || !vehicles.Open(scene) ||
      !drives.Open(scene) ||
      !kinds.Open(scene) ||
      !outshine::Assemble(declared, scene, vehicles, drives, kinds, stood, readError)) {
    std::printf("REFUSED %s\n", readError.c_str());
    return Report();
  }

  outshine::Host::CurlTransport::Config wiring;
  outshine::Host::CurlTransport wire(wiring);
  const bool laid = AssembleDrive(scene, stood, vehicles, drives, declared.Ground, stack, wire,
      outshine::Sim::Provision{"/tmp/outshine-drive-cache", "src/assets", {outshine::Data::ShippedProviders().begin(), outshine::Data::ShippedProviders().end()}}, harness, drive);
  CHECK(laid, "**THE ROAD FROM MARIENPLATZ TO RATHAUSMARKT IS LAID.** A route over ways fetched "
              "live, a corridor fitted through them, the real ground under it shaped to each road "
              "class's own grade, and the declared F31 standing on it -- and every one of those "
              "numbers reported through a SINK, so the windowed driver runs the identical code and "
              "judges nothing");
  if (!laid) { return Report(); }

  CHECK(harness.Seen["how far each walk is as a share of the drive"] < 0.001,
        "**AND THE WALK AT EACH END IS NEGLIGIBLE AGAINST THE DRIVE** -- both squares are "
        "pedestrian zones, the car parks at the carriageway's edge, and the pair of walks stays "
        "under a thousandth of the route. THIS claim lives in the ROUTE-1 CASE now: the engine "
        "publishes the number and asserts nothing city-specific (board:1581's neutrality cut)");
  const double routeKm = drive.Way.Line.LengthM() / 1000.0;
  Note("the route the case itself checks", routeKm, "km");
  CHECK(routeKm > 700.0 && routeKm < 900.0,
        "and its length is what a road between Munich and Hamburg is -- 612 km as the crow "
        "flies, roughly 775 by motorway");
  CHECK(std::fabs(harness.Seen["the elevation where the route starts"] - 523.0) < 40.0 &&
            std::fabs(harness.Seen["the elevation where the route ends"] - 14.0) < 40.0,
        "**AND THE TWO ENDS ARE WHERE THE CITIES ARE**: Munich near 520 m, Hamburg near 10 -- "
        "the check that this is the real world, held by the case that names the cities");

  const auto began = std::chrono::steady_clock::now();
  Ridden rode;
  for (long step = 0; step < kMostSteps; ++step) {
    rode = outshine::Sim::DriveTick(drive.Way, drive.Stood, drive.State, kStepS, nullptr);
    if (!rode.Found || rode.Arrived || rode.Lost || rode.PastLimit || rode.OffTheRoad) { break; }
  }
  const double wallS =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();

  Note("how far the car drove", rode.ReachedM / 1000.0, "km");
  Note("of a corridor this long", drive.Way.Line.LengthM() / 1000.0, "km");
  Note("the fastest it went", rode.TopMs * 3.6, "km/h");
  Note("hours simulated", rode.SimulatedS / 3600.0, "h");
  Note("seconds of wall clock", wallS, "s");
  Note("how much faster than real time", rode.SimulatedS / (wallS > 0.0 ? wallS : 1.0), "x");
  Note("worst deviation from the middle of its own lane", rode.WorstOffsetM, "m");
  Note("where that was", rode.WorstOffsetAtM / 1000.0, "km");
  Note("worst share of a contact's grip used", rode.WorstRatio, "of it");
  Note("most mounts off the ground at once", (double)rode.MostAirborne, "of 4");
  Note("where a contact first went past its limit", rode.BrokeAtM / 1000.0, "km");
  Note("where a wheel first left the carriageway", rode.LeftTheRoadAtM / 1000.0, "km");

  CHECK(!rode.Lost, "the car never left the corridor's own window");
  CHECK(!rode.PastLimit,
        "**AND NO CONTACT WENT PAST ITS DECLARED LIMIT.** A crash on this route is READ -- a load "
        "past what the link carries -- and there was none");
  CHECK(rode.LeftTheRoadAtM <= 0.0,
        "and no wheel ever left the carriageway, which is the road's declared width and not a line");
  CHECK(rode.Arrived,
        "**THE F31 DROVE ITSELF FROM MARIENPLATZ TO RATHAUSMARKT.** Two coordinates in, a route "
        "planned over live OSM ways, a corridor fitted through them, the real ground under it, and "
        "the declared car carried the whole way by four compliant contacts and nothing else");

  stack.Close();

  Covers("I.4.5 the F31 drives itself from Marienplatz to Rathausmarkt over ways fetched live from "
         "the declared OSM source and ground from the declared elevation source, on four compliant "
         "contacts, headless with no renderer linked at all -- and the driving is a shared "
         "translation unit that reports through a sink, so a windowed driver runs the same code");
  return Report();
}
