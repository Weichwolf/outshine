#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
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
    (void)unit;
    Numbers_.push_back(std::string(what) + " = " + std::to_string(value));
    Held_.emplace_back(what, value);
    if (std::string(what).find("steepest gradient") != std::string::npos) { Steepest_ = value; }
  }
  [[nodiscard]] double Steepest() const { return Steepest_; }
  [[nodiscard]] double Of(const char *what) const {
    for (const auto &[said, value] : Held_) {
      if (said == what) { return value; }
    }
    return -1.0;
  }
  [[nodiscard]] const std::vector<std::string> &Numbers() const { return Numbers_; }
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
  double Steepest_ = 0.0;
  std::vector<std::string> Numbers_;
  std::vector<std::pair<std::string, double>> Held_;
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

// board:1784. A zigzag whose legs declare a road class: the corners it forces are legal for the
// car and illegal for the class the way claims to be. Legs of `alongM` with `asideM` of sideways
// throw turn 2*atan(aside/along) at every vertex, and the fit's own byRoom bound puts a radius on
// that turn which no OSM tag ever asked for.
[[nodiscard]] Route Zigzag(int steps, double alongM, double asideM, double minRadiusM) {
  Route out;
  out.Found = true;
  const double perLatDeg = 111320.0;
  const double perLonDeg = 68500.0;
  for (int at = 0; at <= steps; ++at) {
    Leg leg;
    leg.At = Waypoint{52.0 + (double)at * alongM / perLatDeg,
                      9.0 + (at % 2 == 0 ? asideM : -asideM) / perLonDeg};
    leg.AlongM = (double)at * alongM;
    leg.HalfWidthM = 3.5;
    leg.MaxGradient = 0.06;
    leg.MinRadiusM = minRadiusM;
    leg.Lanes = 2;
    out.Legs.push_back(leg);
  }
  out.LengthM = (double)steps * alongM;
  out.StraightM = out.LengthM;
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
    CHECK(quiet.Refused() == 0,
          "**AND A STRAIGHT ROUTE OVER FLAT GROUND REFUSES NOTHING**: the simplest road there "
          "is -- no corner, no slope -- passes every claim the lay publishes, because a claim "
          "bounded per corner has nothing to say about a road without one (board:1792)");
  }

  {
    // board:1624, corrected by measurement: a corridor does not FOLLOW the ground, it is cut
    // and filled into it. Ground rising at 36 % under a route whose legs declare
    // maxGradient = 0.06 produces a road at 6 %, and the climb gate is right to pass it --
    // the wall is earthworks, not a slope the car must climb. My first version of this arm
    // asserted the opposite and was wrong about the engine, not about the engine being wrong.
    FlatGround steep(120.0, 40000.0);
    Quiet quiet;
    Corridor laid;
    std::string why;
    const Route route = Straight(2000.0, 20);
    const bool ok = LayCorridor(route, steep, car, stood, 8.0, stood.TightestM, 52.0,
                                6371008.8, quiet, laid, why);
    Note("the steepest the rig can climb",
         stood.Envelope.DriveN / (stood.Envelope.MassKg * stood.Envelope.GravityMs2) * 100.0, "%");
    Note("the steepest gradient the lay built over a 36 % wall", quiet.Steepest() * 100.0, "%");
    CHECK(ok, "a route over ground far steeper than the car can climb still lays");
    CHECK_NEAR(quiet.Steepest(), 0.06, 1.0e-9, "m/m",
               "**AND THE ROAD IS BUILT TO THE GRADIENT ITS LEGS DECLARE, NOT TO THE GROUND'S**: "
               "the corridor is cut and filled into the terrain, so a 36 % hillside carries a "
               "6 % road and the climb gate has nothing to refuse (board:1624)");
  }

  {
    // and a route whose OWN legs declare a gradient past the drivetrain is the refusal.
    FlatGround flat(120.0);
    Quiet quiet;
    Corridor laid;
    std::string why;
    Route route = Straight(2000.0, 20);
    for (Leg &leg : route.Legs) { leg.MaxGradient = 0.40; }
    const bool ok = LayCorridor(route, flat, car, stood, 8.0, stood.TightestM, 52.0,
                                6371008.8, quiet, laid, why);
    Note("the steepest gradient a 40 % route builds", quiet.Steepest() * 100.0, "%");
    Note("claims it refused", (double)quiet.Refused(), "claims");
    for (const std::string &one : quiet.Refusals()) {
      std::printf("NOTE it refused: %.90s\n", one.c_str());
    }
    CHECK(!ok || quiet.Refused() > 0,
          "**AND A ROUTE THAT DECLARES A CLIMB PAST THE DRIVETRAIN IS REFUSED**: 40 % against "
          "a rig that can pull 23.97 % is a road this car cannot drive, and the lay says so "
          "rather than handing back a plan (board:1624)");
  }

  {
    // board:1784, the box that stood open two rounds: a road class carries a design minimum
    // radius, and the corridor now knows it. RAA 2008 gives EKA 1B 720 m; RAL 2012 gives
    // EKL 1..4 500/400/300/200 m. 400 m is `primary`. The zigzag below is legal for the car --
    // its own centreline minimum is under five metres -- and nowhere near legal for a primary.
    FlatGround flat(120.0);
    Quiet quiet;
    Corridor laid;
    std::string why;
    const Route route = Zigzag(20, 60.0, 15.0, 400.0);
    const bool ok = LayCorridor(route, flat, car, stood, 8.0, stood.TightestM, 52.0,
                                6371008.8, quiet, laid, why);
    const double declared = quiet.Of("vertices whose road class declares a design minimum radius");
    const double under = quiet.Of("corners the fit laid tighter than their class allows");
    Note("vertices carrying a class minimum", declared, "vertices");
    Note("the tightest radius the fit laid", quiet.Of("the tightest radius on it"), "m");
    Note("the car's own centreline minimum", stood.TightestM, "m");
    Note("corners under their class minimum", under, "corners");
    Note("the worst of them", quiet.Of("the worst of them"), "m");
    Note("where its class allows", quiet.Of("where its class allows"), "m");

    CHECK(ok, "a zigzag that is legal for the car still lays");
    CHECK(declared > 0.0,
          "**THE ROAD CLASS REACHES THE FIT**: minRadiusM travels vegetation.json -> Rule -> "
          "Reap -> Network::Lay -> Way -> the node merge -> Leg -> the per-vertex bound, and "
          "the fit is told what class of road it is fitting (board:1784)");
    CHECK(quiet.Of("the tightest radius on it") > stood.TightestM,
          "and every corner on it is one the CAR can drive, so the vehicle bound is silent and "
          "what follows is about the road and not about the rig");
    CHECK(under > 0.0,
          "**AND A CORNER NO ROAD OF THAT CLASS WOULD CARRY IS COUNTED**: a primary is designed "
          "to 400 m and this polyline demands under a hundred, which is a finding about the "
          "graph rather than a corner to smooth -- the vehicle bound could never see it, "
          "because a car turns in five metres (board:1784)");
    CHECK_NEAR(quiet.Of("where its class allows"), 400.0, 1.0e-9, "m",
               "and the minimum it names is the one the way's own class declares, not a "
               "constant beside the fit");
    CHECK(quiet.Of("the worst of them") < 400.0 &&
              quiet.Of("the worst of them") >= quiet.Of("the tightest radius on it"),
          "and the worst of them is a radius the fit actually laid");
  }

  {
    // the same shape with the class taken off the legs. This is the negative control, standing
    // in the proof: nothing about the geometry changed, only what the way claims to be.
    FlatGround flat(120.0);
    Quiet quiet;
    Corridor laid;
    std::string why;
    const Route route = Zigzag(20, 60.0, 15.0, 0.0);
    const bool ok = LayCorridor(route, flat, car, stood, 8.0, stood.TightestM, 52.0,
                                6371008.8, quiet, laid, why);
    Note("vertices carrying a class minimum, class removed",
         quiet.Of("vertices whose road class declares a design minimum radius"), "vertices");
    Note("corners under their class minimum, class removed",
         quiet.Of("corners the fit laid tighter than their class allows"), "corners");
    CHECK(ok, "the same zigzag without a class still lays");
    CHECK(quiet.Of("corners the fit laid tighter than their class allows") == 0.0,
          "**AND A WAY THAT DECLARES NO CLASS IS BOUNDED BY NOTHING BUT THE CAR**: the kinds "
          "below tertiary are RASt 06 territory and this tree has fetched no minimum for them, "
          "so they carry none -- an absent number is absent, never a zero that refuses "
          "everything (board:1784)");
  }

  {
    // and a straight road of the same class: the bound exists and does not fire.
    FlatGround flat(120.0);
    Quiet quiet;
    Corridor laid;
    std::string why;
    Route route = Straight(2000.0, 20);
    for (Leg &leg : route.Legs) { leg.MinRadiusM = 400.0; }
    const bool ok = LayCorridor(route, flat, car, stood, 8.0, stood.TightestM, 52.0,
                                6371008.8, quiet, laid, why);
    Note("vertices carrying a class minimum on the straight",
         quiet.Of("vertices whose road class declares a design minimum radius"), "vertices");
    Note("corners under their class minimum on the straight",
         quiet.Of("corners the fit laid tighter than their class allows"), "corners");
    CHECK(ok && quiet.Refused() == 0,
          "a straight primary lays and refuses nothing");
    CHECK(quiet.Of("vertices whose road class declares a design minimum radius") > 0.0 &&
              quiet.Of("corners the fit laid tighter than their class allows") == 0.0,
          "**AND A ROAD THAT OBEYS ITS CLASS IS NOT ACCUSED OF ANYTHING**: the bound is armed "
          "on every vertex and counts nothing, which is what separates an instrument from a "
          "claim that is true because it never looks (board:1784)");
  }

  Covers("II.15 LayCorridor is reachable by a unit case: it asks the ground two questions "
         "through a door a synthetic one can answer, so the width tables, the grade walk, "
         "the climb gate and the profile step are in the fast gate (board:1624)");
  return Report();
}
