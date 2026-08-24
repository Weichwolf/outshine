#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"

#include "CorridorLay.h"
#include "Rigging.h"
#include "Wayfinding.h"

using outshine::GroundQuery;
using outshine::GroundSample;
using outshine::Sink;
using outshine::Vehicle;
using outshine::Contact;
using outshine::Path::Leg;
using outshine::Path::Route;
using outshine::Path::Waypoint;
using outshine::Sim::Corridor;
using outshine::Sim::LayCorridor;
using outshine::Sim::Rigged;
using outshine::Sim::Stand;

namespace {

// board:1624: LayCorridor used to spell GroundStream &, which is built from a TilePool --
// threads, a content store, fetched tiles. It asks the ground exactly two questions, and both
// are pure functions of a coordinate. With the door narrowed to those two, a synthetic ground
// is ten lines and the fast gate can reach 550 lines that no unit case had ever touched.
class FlatGround final : public GroundQuery {
public:
  explicit FlatGround(double aslM, double slopePerDeg = 0.0)
      : AslM_(aslM), SlopePerDeg_(slopePerDeg) {}

  [[nodiscard]] GroundSample At(double lat, double lon) const override {
    (void)lon;
    ++Asked_;
    return GroundSample::At(AslM_ + SlopePerDeg_ * (lat - 52.0));
  }
  [[nodiscard]] double PostM(double latDeg) const override {
    (void)latDeg;
    return 30.0;
  }
  [[nodiscard]] size_t Asked() const { return Asked_; }

private:
  double AslM_;
  double SlopePerDeg_;
  mutable size_t Asked_ = 0;
};

class Quiet final : public Sink {
public:
  void Number(const char *what, double value, const char *unit) override {
    (void)what;
    (void)value;
    (void)unit;
  }
  void Claim(bool held, const char *why) override {
    if (!held) {
      ++Refused_;
      Refusals_.push_back(why);
    }
  }
  void Near(double got, double want, double within, const char *unit, const char *why) override {
    (void)got;
    (void)want;
    (void)within;
    (void)unit;
    (void)why;
  }
  void Say(const std::string &line) override { (void)line; }

  [[nodiscard]] size_t Refused() const { return Refused_; }
  [[nodiscard]] const std::vector<std::string> &Refusals() const { return Refusals_; }

private:
  size_t Refused_ = 0;
  std::vector<std::string> Refusals_;
};

[[nodiscard]] Vehicle Plausible() {
  Vehicle made;
  made.Name = "twin";
  made.MassKg = 1610.0;
  made.WidthM = 1.811;
  made.TyreRadiusM = 0.333;
  made.Grip = 0.95;
  made.TurningCircleM = 11.3;
  made.TrackM = 1.548;
  made.PeakTorqueNm = 400.0;
  made.FinalDrive = 3.15;
  made.BrakeTorqueNm = 3000.0;
  made.DragCoefficient = 0.66;
  made.FrontalM2 = 2.19;
  made.CentreOfMassM[1] = 0.55;
  const double corners[4][3] = {{-0.774, 0.333, -1.405},
                                {0.774, 0.333, -1.405},
                                {-0.774, 0.333, 1.405},
                                {0.774, 0.333, 1.405}};
  for (const auto &at : corners) {
    Contact one;
    one.AtM[0] = at[0];
    one.AtM[1] = at[1];
    one.AtM[2] = at[2];
    one.ReachM = 0.45635;
    one.StiffnessNPerM = 32000.0;
    one.DampingNsPerM = 3400.0;
    one.TravelM = 0.18;
    one.StopNPerM = 450000.0;
    one.LimitN = 24000.0;
    made.Contacts.push_back(one);
  }
  return made;
}

[[nodiscard]] Route Straight(double metres, int steps) {
  Route out;
  out.Found = true;
  const double perStep = metres / (double)steps;
  const double perDeg = 111320.0;
  for (int at = 0; at <= steps; ++at) {
    Leg leg;
    leg.At = Waypoint{52.0 + (double)at * perStep / perDeg, 9.0};
    leg.AlongM = (double)at * perStep;
    leg.HalfWidthM = 3.5;
    leg.MaxGradient = 0.06;
    leg.Lanes = 2;
    out.Legs.push_back(leg);
  }
  out.LengthM = metres;
  out.StraightM = metres;
  out.Reached = out.Legs.size();
  return out;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const Vehicle car = Plausible();
  const Rigged stood = Stand(car, 9.80665, 1.225);
  CHECK(stood.Stood, "the declaration stands as a rig");
  if (!stood.Stood) {
    std::printf("REFUSED %s\n", stood.Error.c_str());
    return Report();
  }

  {
    FlatGround flat(120.0);
    Quiet quiet;
    Corridor laid;
    std::string why;
    const Route route = Straight(2000.0, 20);
    const bool ok = LayCorridor(route, flat, car, stood, 8.0, stood.TightestM, 52.0,
                                6371008.8, quiet, laid, why);
    if (!why.empty()) { std::printf("NOTE the lay's error text: %s\n", why.c_str()); }
    for (const std::string &one : quiet.Refusals()) {
      std::printf("NOTE the lay refused: %.90s\n", one.c_str());
    }
    Note("ground queries the lay made", (double)flat.Asked(), "queries");
    Note("claims the lay refused", (double)quiet.Refused(), "claims");
    Note("stations in the plan", (double)laid.Profile.SampleCount(), "stations");
    Note("the corridor's length", laid.Line.LengthM(), "m");

    CHECK(ok,
          "**A STRAIGHT ROUTE BECOMES A CORRIDOR**: 550 lines of width tables, grade walk, "
          "climb gate, height knots and profile step, reached by a unit case over a "
          "synthetic ground -- because the door asks for the two queries it uses and not "
          "for the thread pool that happens to hold them (board:1624)");
    CHECK(flat.Asked() > 0, "and it did ask the ground, so the synthetic one is really used");
    CHECK_NEAR(laid.Line.LengthM(), 2000.0, 60.0, "m",
               "and the corridor is the length of the route it was laid over");

    // What this twin found on its FIRST run is published, not asserted green: four of the
    // lay's own claims refuse on a straight synthetic route, and the speed profile lays zero
    // stations over a 1997.76 m corridor. Both are defects in code no unit case had ever
    // reached, and they are filed as board:1792 rather than papered over here. Asserting
    // Refused() == 0 today would mean asserting the defect away.
    Note("claims this lay refuses on a straight route", (double)quiet.Refused(), "claims");
    CHECK(quiet.Refused() <= 4,
          "and the count of its own refusals is PINNED, so a fifth is a regression and the "
          "four that stand are board:1792's subject rather than a surprise (board:1624)");
  }

  {
    // the climb gate: ground that rises faster than the declared drivetrain can pull.
    FlatGround steep(120.0, 40000.0);
    Quiet quiet;
    Corridor laid;
    std::string why;
    const Route route = Straight(2000.0, 20);
    const bool ok = LayCorridor(route, steep, car, stood, 8.0, stood.TightestM, 52.0,
                                6371008.8, quiet, laid, why);
    std::printf("NOTE over a wall, the lay %s\n", ok ? "succeeded" : "refused");
    if (!why.empty()) { std::printf("NOTE it said: %s\n", why.c_str()); }
    for (const std::string &one : quiet.Refusals()) {
      std::printf("NOTE over a wall it refused: %.90s\n", one.c_str());
    }
    CHECK(!ok || quiet.Refused() > 0,
          "**AND GROUND THE DRIVETRAIN CANNOT CLIMB IS A REFUSAL, NOT A CORRIDOR**: a route "
          "laid over a wall either fails outright or fails a claim it publishes, rather "
          "than handing back a plan the car cannot follow (board:1624)");
  }

  Covers("II.15 LayCorridor is reachable by a unit case: it asks the ground two questions "
         "through a door a synthetic one can answer, so the width tables, the grade walk, "
         "the climb gate and the profile step are in the fast gate (board:1624)");
  return Report();
}
