#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"

#include "Drive.h"
#include "Fly.h"
#include "Rail.h"
#include "Walk.h"

using outshine::Curve;
using outshine::Envelope;
using outshine::Knot;
using outshine::Placed;
using outshine::ReferenceLine;
using outshine::Segment;
using outshine::Pilot::Attitude;
using outshine::Pilot::Axles;
using outshine::Pilot::BankLimitOf;
using outshine::Pilot::Demand;
using outshine::Pilot::Drive;
using outshine::Pilot::Fly;
using outshine::Pilot::Gait;
using outshine::Pilot::Haul;
using outshine::Pilot::Hold;
constexpr double kGravityMs2 = 9.80665;
using outshine::Pilot::Locate;
using outshine::Pilot::OverturningMs2;
using outshine::Pilot::Placement;
using outshine::Pilot::Rails;
using outshine::Pilot::Reins;
using outshine::Pilot::Ride;
using outshine::Pilot::Steering;
using outshine::Pilot::Stride;
using outshine::Pilot::TightestPerM;
using outshine::Pilot::Walk;
using outshine::Pilot::Wings;

namespace {

constexpr double kRadiusM = 400.0;
constexpr double kCurvature = 1.0 / kRadiusM;
constexpr double kCantRad = 0.06;
constexpr double kGaugeM = 1.435;
constexpr double kRailCentreM = 1.8;
constexpr double kCantDeficiencyMs2 = 1.0;
constexpr double kTrainMs = 30.0;
constexpr double kWalkMs = 1.4;
constexpr double kTurnRateRadS = 1.5;
constexpr double kFlyMs = 80.0;
constexpr double kBankLimitRad = 0.4363323129985824;
constexpr double kLoadFactorLimit = 2.0;
constexpr double kClimbLimitRad = 0.1745329251994330;

ReferenceLine Curved(std::string &error) {
  ReferenceLine line;
  const std::vector<Segment> along = {{Curve::Arc, 1000.0, kCurvature, kCurvature}};
  if (!line.Lay(Placed{}, along, error)) { return line; }
  const std::vector<Knot> cant = {{0.0, kCantRad, 0.0}, {1000.0, kCantRad, 0.0}};
  if (!line.Bank(cant, error)) { return ReferenceLine(); }
  return line;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  std::string error;
  ReferenceLine line = Curved(error);
  if (!(line.LengthM() > 0.0)) {
    std::printf("REFUSED %s\n", error.c_str());
    return Report();
  }

  Placed on;
  if (!line.At(400.0, on)) { return Report(); }
  const Placement at = Locate(line, on.EastM, on.NorthM, on.HeightM, on.HeadingRad, 400.0, 40.0);
  CHECK(at.Found && std::fabs(at.OffsetM) < 1.0e-6,
        "one vehicle, exactly on one curve, and every mode below is asked the same question about "
        "it");
  CHECK_NEAR(at.BankRad, kCantRad, 1.0e-12, "rad",
             "which is banked, because the same profile that superelevates a road cants a railway");

  Reins reins;
  reins.SettleS = 1.0;
  reins.LeastReachM = 3.0;
  const Demand asked = Hold(line, reins, at, kTrainMs, kTrainMs);
  Note("the curvature the pilot demands", asked.CurvaturePerM, "1/m");
  CHECK_NEAR(asked.CurvaturePerM, kCurvature, 1.0e-9, "1/m",
             "**AND THE DEMAND IS ONE NUMBER: A CURVATURE.** Everything below this line is a "
             "conversion, and none of it can reach back up -- which is what stops a mode's noun "
             "from appearing in the mechanism that serves all of them");

  Envelope car;
  car.Grip = 0.95;
  car.GravityMs2 = kGravityMs2;
  car.MassKg = 1610.0;
  car.DriveN = 400.0 * 3.08 / 0.333;
  car.BrakeN = 2200.0 / 0.333;
  car.DragArea = 0.66 * 2.19;
  car.AirDensity = 1.225;
  Axles axles;
  axles.WheelbaseM = 2.810;
  axles.SteerLimitRad = 0.55;
  const Steering wheel = Drive(axles, car, asked);
  Note("a CAR turns the wheel", wheel.SteerRad, "rad");
  CHECK_NEAR(wheel.SteerRad, std::atan(axles.WheelbaseM * kCurvature), 1.0e-12, "rad",
             "a car converts it with atan(L kappa) -- Ackermann, and its limit is the lesser of the "
             "rack and the grip");

  Gait gait;
  gait.TurnRateRadS = kTurnRateRadS;
  gait.TopMs = 1.6;
  gait.AccelMs2 = 1.0;
  const Stride step = Walk(gait, asked, kWalkMs);
  Note("a WALKER turns its body", step.HeadingRateRadS, "rad/s");
  Note("the tightest circle a walker can hold at 1.4 m/s", 1.0 / TightestPerM(gait, kWalkMs), "m");
  CHECK_NEAR(step.HeadingRateRadS, kCurvature * kWalkMs, 1.0e-12, "rad/s",
             "**A WALKER CONVERTS IT WITH v KAPPA AND HAS NO WHEELBASE AT ALL**, which is the point: "
             "the base cannot contain a wheelbase, a bank angle or a rail, because three of its "
             "four users do not have one");
  CHECK_NEAR(1.0 / TightestPerM(gait, kWalkMs), kWalkMs / kTurnRateRadS, 1.0e-12, "m",
             "and its tightest circle is its speed over its turn rate -- 0.93 m at walking pace, "
             "which is why a person rounds a corner and a car does not");
  CHECK(TightestPerM(gait, 0.0) == 0.0,
        "with no limit at all when it is standing still, because a person turns on the spot and a "
        "limit that pretended otherwise would be a fact about cars leaking into legs");

  Envelope aircraft;
  aircraft.GravityMs2 = kGravityMs2;
  aircraft.MassKg = 1200.0;
  aircraft.DriveN = 4000.0;
  aircraft.DragArea = 0.9;
  aircraft.AirDensity = 1.0;
  Wings wings;
  wings.BankLimitRad = kBankLimitRad;
  wings.LoadFactorLimit = kLoadFactorLimit;
  wings.ClimbLimitRad = kClimbLimitRad;

  Demand tooTight = asked;
  tooTight.SpeedMs = kFlyMs;
  const Attitude leaning = Fly(wings, aircraft, tooTight);
  Note("the bank a 400 m curve would need at 80 m/s",
       std::atan(kFlyMs * kFlyMs * kCurvature / kGravityMs2), "rad");
  Note("the bank this aircraft is allowed", BankLimitOf(wings), "rad");
  Note("the tightest circle that leaves it at 80 m/s", 1.0 / TightestPerM(wings, kFlyMs, kGravityMs2), "m");
  CHECK_NEAR(leaning.BankRad, BankLimitOf(wings), 1.0e-12, "rad",
             "**AND THE FIRST THING THE AIRCRAFT SAYS ABOUT THE CAR'S CURVE IS THAT IT CANNOT FLY "
             "IT.** 400 m at 80 m/s wants 58 degrees of bank; it has 25, so its tightest circle is "
             "1399 m. One demand, four modes, and three of them can do it -- which is exactly the "
             "kind of thing a shared currency makes VISIBLE instead of unaskable");

  const double flyablePerM = 3.0e-4;
  Demand flying = asked;
  flying.SpeedMs = kFlyMs;
  flying.CurvaturePerM = flyablePerM;
  flying.ClimbRad = 0.05;
  const Attitude attitude = Fly(wings, aircraft, flying);
  Note("an AIRCRAFT banks", attitude.BankRad, "rad");
  Note("what atan(v^2 kappa / g) says", std::atan(kFlyMs * kFlyMs * flyablePerM / kGravityMs2),
       "rad");
  Note("the load factor that costs", attitude.LoadFactor, "g");

  CHECK_NEAR(attitude.BankRad, std::atan(kFlyMs * kFlyMs * flyablePerM / kGravityMs2), 1.0e-12,
             "rad",
             "**AN AIRCRAFT CONVERTS IT WITH atan(v^2 kappa / g), WHICH IS A BANK AND NOT A "
             "STEER.** Nothing about the base changed to make that possible, because the base never "
             "knew what a wheel was");
  CHECK_NEAR(attitude.LoadFactor, 1.0 / std::cos(attitude.BankRad), 1.0e-12, "g",
             "and it pays for the turn in load factor, 1/cos of the bank, which is the currency an "
             "airframe actually breaks in");

  Note("the bank its load factor limit allows on its own", std::acos(1.0 / kLoadFactorLimit),
       "rad");
  CHECK_NEAR(BankLimitOf(wings), kBankLimitRad, 1.0e-12, "rad",
             "so the bank it may use is the lesser of what the structure allows and what acos(1/n) "
             "allows, and on THIS airframe the 25 degree structure binds -- 2 g would have "
             "permitted 60");

  Wings strong = wings;
  strong.BankLimitRad = 1.3962634015954636;
  Note("the bank a stronger wing would be allowed", BankLimitOf(strong), "rad");
  CHECK_NEAR(BankLimitOf(strong), std::acos(1.0 / kLoadFactorLimit), 1.0e-12, "rad",
             "and give it 80 degrees of structure and the LOAD FACTOR binds instead, at exactly "
             "acos(1/n) -- a limit derived from the other declaration rather than written twice and "
             "left to disagree");

  CHECK_NEAR(attitude.ClimbRad, 0.05, 1.0e-12, "rad",
             "**AND IT IS THE ONLY MODE THAT COMMANDS THE VERTICAL.** The same sighting that gives a "
             "car its curvature gives an aircraft its climb angle, and a car simply does not read "
             "that field -- the ground decides its height");

  Rails rails;
  rails.CantDeficiencyMs2 = kCantDeficiencyMs2;
  rails.GaugeM = kGaugeM;
  rails.CentreOfMassM = kRailCentreM;
  Envelope train;
  train.GravityMs2 = kGravityMs2;
  train.MassKg = 80000.0;
  train.DriveN = 60000.0;
  train.BrakeN = 90000.0;
  train.Grip = 0.15;
  train.DragArea = 8.0;
  train.AirDensity = 1.225;

  const Haul haul = Ride(rails, train, asked, at, kTrainMs);
  const double unbalanced = kTrainMs * kTrainMs * kCurvature - kGravityMs2 * std::sin(kCantRad);
  Note("a TRAIN converts none of it, and reads this instead", haul.UnbalancedMs2, "m/s2");
  Note("what v^2 kappa - g sin(cant) says", unbalanced, "m/s2");
  Note("the cant deficiency it is allowed", kCantDeficiencyMs2, "m/s2");
  Note("the lateral acceleration that would tip it", OverturningMs2(rails, kGravityMs2), "m/s2");

  CHECK_NEAR(haul.UnbalancedMs2, unbalanced, 1.0e-12, "m/s2",
             "**A TRAIN CANNOT STEER AT ALL, AND THAT IS THE CASE THAT PROVES THE SHAPE.** It "
             "consumes the same demand and converts the lateral half of it into NOTHING: the "
             "curvature is the rail's, so what the mode publishes is the unbalanced lateral "
             "acceleration the passengers feel -- v^2 kappa less what the cant already carries");
  CHECK(haul.PastCant,
        "and at 30 m/s on a 400 m curve canted 0.06 rad it is past what a railway allows, which is "
        "a REFUSAL OF THE SPEED and not of the path -- the only channel a train has is how fast");
  CHECK(!haul.Overturns,
        "though nowhere near tipping it over, so the two declarations are separately readable and "
        "a train that is merely uncomfortable is not reported as a train that has crashed");

  const double allowedMs =
      std::sqrt((kCantDeficiencyMs2 + kGravityMs2 * std::sin(kCantRad)) / kCurvature);
  Note("the speed this curve actually allows", allowedMs, "m/s");
  Note("the same speed", allowedMs * 3.6, "km/h");
  const Haul slower = Ride(rails, train, asked, at, allowedMs);
  CHECK(!slower.PastCant,
        "**AND THAT SPEED IS DERIVED FROM THE CURVE AND THE CANT, NOT LOOKED UP.** sqrt((a + g "
        "sin(cant)) / kappa) is 91 km/h here, and a route planner that asks the corridor rather "
        "than a table gets the right answer on a curve nobody has ever driven");
  CHECK_NEAR(OverturningMs2(rails, kGravityMs2), kGravityMs2 * 0.5 * kGaugeM / kRailCentreM, 1.0e-12, "m/s2",
             "and the tipping limit is the gauge and the centre of mass, which is a lever and not a "
             "constant");

  Covers("I.9.6 one pilot serves every way of moving: it demands a curvature, and a car converts it "
         "to a steering angle, a walker to a turn rate, an aircraft to a bank and a climb, and a "
         "train to nothing at all -- each deriving its own limit from its own declaration, and none "
         "of them spellable in the base");
  return Report();
}
