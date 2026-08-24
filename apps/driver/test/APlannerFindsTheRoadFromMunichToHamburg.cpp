#include <chrono>
#include <cmath>
#include <numbers>
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

}

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
  // board:1812: the corridor reserves budgetM between the aim and the edge for exactly this
  // error, and one drive's worst is one sample. The distribution is what a reserve may be
  // derived from.
  {
    const auto quantile = [&](double share) {
      const long want = (long)(share * (double)rode.OffsetSamples);
      long seen = 0;
      for (size_t bin = 0; bin < outshine::Sim::Ridden::kOffsetBins; ++bin) {
        seen += (long)rode.OffsetBin[bin];
        if (seen >= want) { return ((double)bin + 0.5) * outshine::Sim::Ridden::kOffsetBinM; }
      }
      return (double)outshine::Sim::Ridden::kOffsetBins * outshine::Sim::Ridden::kOffsetBinM;
    };
    Note("stations the deviation was sampled at", (double)rode.OffsetSamples, "samples");
    Note("p50 of the deviation", quantile(0.50), "m");
    Note("p95", quantile(0.95), "m");
    Note("p99", quantile(0.99), "m");
    Note("the reserve the corridor keeps for it", drive.Way.BudgetM, "m");
    Note("p99 as a share of that reserve", quantile(0.99) / drive.Way.BudgetM, "of it");
    Note("the worst single sample as a share of p99", std::fabs(rode.WorstOffsetM) / quantile(0.99),
         "x");
    Note("where it was last calm before that worst", rode.CalmBeforeWorstAtM / 1000.0, "km");
    Note("how far the excursion ran", rode.WorstOffsetAtM - rode.CalmBeforeWorstAtM, "m");
    Note("the aim where it was calm", rode.AimAtCalmM, "m");
    Note("the aim at the worst", rode.AimAtWorstM, "m");
    Note("how far the aim moved between them", rode.AimAtWorstM - rode.AimAtCalmM, "m");
    CHECK(quantile(0.99) < drive.Way.BudgetM,
          "**AND THE LANE THE CORRIDOR RESERVES IS THE LANE THE CAR KEEPS**: budgetM is the "
          "room the corridor holds between the lane centre it aims at and the edge, and a "
          "drive whose p99 spends it has no reserve left for the tail -- the bar is the "
          "distribution, not the worst sample, because one drive's worst is one sample and a "
          "reserve fitted to it would be calibration deciding (board:1812)");
  }
  Note("where that was", rode.WorstOffsetAtM / 1000.0, "km");
  Note("worst share of a contact's grip used", rode.WorstRatio, "of it");
  Note("where it asked for that", rode.WorstRatioAtM / 1000.0, "km");
  Note("where a tyre first let go", rode.Slid ? rode.SlidFirstAtM / 1000.0 : -1.0, "km");
  Note("how far it slid", rode.SlidM / 1000.0, "km");
  Note("the share of the route it slid over",
       rode.ReachedM > 0.0 ? rode.SlidM / rode.ReachedM : 0.0, "of it");
  CHECK(rode.SlidM <= rode.ReachedM && (!rode.Slid || rode.SlidFirstAtM >= 0.0),
        "**A TYRE THAT LET GO IS REPORTED WITH ITS STATION**, the way a wheel leaving the "
        "road is -- a worst share of grip with no place to look at is a number nobody can "
        "act on (board:1772)");
  // board:1772's second box. A bar on the PEAK ratio is the wrong instrument: a tyre at its
  // limit in a bend is ordinary driving, and any threshold on the peak is a number fitted to
  // whichever run produced it. What can be bounded without fitting is the SHAPE of the answer.
  //
  // The speed profile reserves lateral acceleration for holding the line, and the pilot spends
  // it inside the plan. Sliding means the pilot left the plan -- so it may happen, and it may
  // not be a feature of the route. kSlidShare [SET] = 1/1000 bounds it as VANISHING rather than
  // appreciable: it is two orders of magnitude above what this route measures, so it constrains
  // the shape and is not a fit to the measurement. The headroom is published beside it, and a
  // repair that made sliding routine would spend that headroom before it went red.
  constexpr double kSlidShare = 0.001;
  const double slidShare = rode.ReachedM > 0.0 ? rode.SlidM / rode.ReachedM : 0.0;
  Note("the share of the route a tyre may slide over", kSlidShare, "of it");
  Note("the headroom that leaves", slidShare > 0.0 ? kSlidShare / slidShare : 0.0, "x");
  CHECK(slidShare < kSlidShare,
        "**AND SLIDING IS A VANISHING SHARE OF THE ROUTE, NOT A FEATURE OF IT**: the profile "
        "reserves lateral acceleration so the pilot can hold the line inside the plan, and a "
        "drive that spends an appreciable part of its distance past the friction circle is a "
        "drive whose plan the pilot is not following -- the bound is on the SHAPE of that "
        "answer, which is why it is not the peak ratio it would be fitted to (board:1772)");
  Note("most mounts off the ground at once", (double)rode.MostAirborne, "of 4");
  Note("where a contact first went past its limit", rode.BrokeAtM / 1000.0, "km");
  Note("where a wheel first left the carriageway", rode.LeftTheRoadAtM / 1000.0, "km");
  // board:1767: the tick already knows why -- the lane it was in, how far from its middle, how
  // fast it was going against what the plan asked for, and how hard the road was turning there.
  // The case published the station alone, which is a fault with a place and no attribution.
  if (rode.LeftTheRoadAtM > 0.0) {
    Note("how far from its lane's middle it was", rode.LeftByM, "m");
    Note("the lane it was in", rode.LeftLaneM, "m");
    Note("what that lane leaves either side of the car", 0.5 * (rode.LeftLaneM - drive.Car.WidthM),
         "m");
    Note("how much of that margin it had spent", std::fabs(rode.LeftByM) /
         (0.5 * (rode.LeftLaneM - drive.Car.WidthM)), "of it");
    Note("how fast it was going", rode.LeftAtMs * 3.6, "km/h");
    Note("what the plan asked for there", rode.LeftPlannedMs * 3.6, "km/h");
    Note("how hard the road was turning", rode.LeftCurvature, "1/m");
    Note("the radius that is", rode.LeftCurvature != 0.0 ? 1.0 / std::fabs(rode.LeftCurvature)
                                                         : 0.0, "m");
    Note("how fast the turn was tightening", rode.LeftRate, "1/m2");
    Note("where the corridor's own edge was", rode.LeftEdgeM, "m");
    Note("what the pilot was aiming for", rode.LeftAsideM, "m");
    Note("where the car actually was", rode.LeftAcrossM, "m");
    // board:1767's two named measurements: whether the steer COMMAND is wrong, or whether the
    // car does not follow it.
    Note("the steer the pilot commanded", rode.LeftSteerRad * 180.0 / std::numbers::pi, "deg");
    Note("what a kinematic bicycle needs for that curvature",
         rode.LeftKinematicSteerRad * 180.0 / std::numbers::pi, "deg");
    Note("the command as a share of it",
         rode.LeftKinematicSteerRad != 0.0 ? rode.LeftSteerRad / rode.LeftKinematicSteerRad : 0.0,
         "of it");
    Note("the worst front slip angle there",
         rode.LeftFrontSlipRad * 180.0 / std::numbers::pi, "deg");
    Note("the worst rear slip angle", rode.LeftRearSlipRad * 180.0 / std::numbers::pi, "deg");
    Note("how far the aim still had to travel", rode.LeftAimStillMovingM, "m");
    Note("the fastest the aim may move sideways at that speed",
         drive.State.AsideRatePerM * rode.LeftAtMs, "m/s");
    Note("how the road was banked there", rode.LeftBankRad * 180.0 / std::numbers::pi, "deg");
    Note("the sideways gravity that bank puts on the car",
         9.80665 * std::sin(rode.LeftBankRad), "m/s2");
    Note("the lateral acceleration the corner itself asks for",
         rode.LeftAtMs * rode.LeftAtMs * std::fabs(rode.LeftCurvature), "m/s2");
    Note("the slope there", rode.LeftSlope * 100.0, "%");
    Note("where the excursion began", rode.StrayedAtM / 1000.0, "km");
    Note("how far before the crossing that is",
         (rode.LeftTheRoadAtM - rode.StrayedAtM), "m");
    Note("how hard the road was turning there", rode.StrayedCurvature, "1/m");
    Note("the radius that is", rode.StrayedCurvature != 0.0
                                   ? 1.0 / std::fabs(rode.StrayedCurvature) : 0.0, "m");
    Note("how fast the turn was tightening there", rode.StrayedRate, "1/m2");
    Note("how fast it was going there", rode.StrayedAtMs * 3.6, "km/h");
    Note("what the plan asked for there", rode.StrayedPlannedMs * 3.6, "km/h");
    Note("the lateral acceleration that corner asked for",
         rode.StrayedAtMs * rode.StrayedAtMs * std::fabs(rode.StrayedCurvature), "m/s2");
    Note("the car's offset where the excursion began", rode.StrayedOffsetM, "m");
    Note("and where it crossed", rode.LeftAcrossM, "m");
    Note("the offset moved by", rode.LeftAcrossM - rode.StrayedOffsetM, "m");
    Note("the angle that implies against the path",
         std::atan2(std::fabs(rode.LeftAcrossM - rode.StrayedOffsetM),
                    rode.LeftTheRoadAtM - rode.StrayedAtM) * 180.0 / std::numbers::pi, "deg");
    Note("the heading error where it began",
         rode.StrayedHeadingErrorRad * 180.0 / std::numbers::pi, "deg");
    Note("the heading error where it crossed",
         rode.LeftHeadingErrorRad * 180.0 / std::numbers::pi, "deg");
    Note("what the corridor wanted the aim to be", rode.LeftWantAsideM, "m");
    Note("the room the clamp allowed", rode.LeftRoomM, "m");
    Note("the corridor's half width there", rode.LeftHalfWidthM, "m");
    Note("the lane centre that half width and 2 lanes imply", -0.5 * rode.LeftHalfWidthM, "m");
    {
      const double fineM = drive.Way.FineM, spanM = drive.Way.SpanM;
      const size_t fine = (size_t)(rode.LeftTheRoadAtM / fineM);
      const size_t post = (size_t)(rode.LeftTheRoadAtM / spanM);
      Note("the fine step", fineM, "m");
      Note("the coarse step", spanM, "m");
      Note("fine index", (double)fine, "");
      Note("coarse index", (double)post, "");
      if (fine < drive.Way.FineAside.size()) {
        Note("FineAside there", drive.Way.FineAside[fine], "m");
        Note("FineLaneHalfM there", drive.Way.FineLaneHalfM[fine], "m");
        Note("FineEdge there", drive.Way.FineEdge[fine], "m");
      }
      if (post < drive.Way.AsideM.size()) {
        Note("AsideM at the coarse post", drive.Way.AsideM[post], "m");
        Note("LaneHalfM at the coarse post", drive.Way.LaneHalfM[post], "m");
        Note("HalfWidthM at the coarse post", drive.Way.HalfWidthM[post], "m");
      }
    }
    Note("the slip at which this tyre reaches peak force",
         0.95 * (drive.Car.MassKg * 9.80665 / 4.0) / 55000.0 * 180.0 / std::numbers::pi, "deg");
  }

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
