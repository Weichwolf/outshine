#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "Check.h"

#include "Body.h"
#include "Carriageway.h"
#include "DriveTick.h"
#include "ReferenceLine.h"
#include "Rigging.h"

using outshine::Contact;
using outshine::Curve;
using outshine::Placed;
using outshine::ReferenceLine;
using outshine::Segment;
using outshine::Standing;
using outshine::Vehicle;
using outshine::Sim::Corridor;
using outshine::Sim::DriveState;
using outshine::Sim::DriveTick;
using outshine::Sim::Ridden;
using outshine::Sim::Rigged;
using outshine::Sim::Stand;

namespace {

constexpr double kEarthMs2 = 9.80665;
constexpr double kMoonMs2 = 1.62;
constexpr double kRoadM = 300.0;
constexpr double kLaneHalfM = 1.75;
constexpr double kEdgeM = 3.5;
constexpr double kDtS = 1.0 / 60.0;
constexpr double kJoinMs = 20.0;

Vehicle Plausible() {
  Vehicle out;
  out.MassKg = 1000.0;
  out.WidthM = 1.8;
  out.WheelbaseM = 2.5;
  out.TurningCircleM = 11.0;
  out.Grip = 0.9;
  out.TyreRadiusM = 0.3;
  out.CorneringNPerRad = 50000.0;
  out.RelaxationM = 0.4;
  out.PeakTorqueNm = 300.0;
  out.FinalDrive = 3.0;
  out.BrakeTorqueNm = 5000.0;
  out.DragCoefficient = 0.3;
  out.FrontalM2 = 2.2;
  out.CentreOfMassM[1] = 0.5;
  out.InertiaKgM2[0] = 1500.0;
  out.InertiaKgM2[1] = 1800.0;
  out.InertiaKgM2[2] = 400.0;
  const double reach = 0.45;
  for (int corner = 0; corner < 4; ++corner) {
    Contact one;
    one.AtM[0] = corner % 2 == 0 ? -0.75 : 0.75;
    one.AtM[1] = 0.3;
    one.AtM[2] = corner < 2 ? -1.25 : 1.25;
    one.ReachM = reach;
    one.StiffnessNPerM = 30000.0;
    one.DampingNsPerM = 3000.0;
    one.TravelM = 0.18;
    one.StopNPerM = 400000.0;
    one.LimitN = 20000.0;
    out.Contacts.push_back(one);
  }
  return out;
}

bool Straight(Corridor &way, const Rigged &stood, double edgeM, std::string &error) {
  if (!way.Line.Lay(Placed{}, {{Curve::Straight, kRoadM, 0.0, 0.0}}, error)) { return false; }
  if (!way.Profile.Over(way.Line, stood.Envelope, 0.5, kJoinMs, error)) { return false; }
  way.SpanM = 25.0;
  const size_t posts = (size_t)(kRoadM / way.SpanM) + 2u;
  const size_t fine = (size_t)(kRoadM / way.FineM) + 2u;
  way.LaneHalfM.assign(posts, kLaneHalfM);
  way.FineAside.assign(fine, 0.0);
  way.FineEdge.assign(fine, edgeM);
  way.NarrowestLaneM = 2.0 * kLaneHalfM;
  way.BudgetM = 0.2;
  way.HoldWithinM = 0.3;
  return true;
}

bool Seat(DriveState &drive, const Corridor &way, const Vehicle &car, const Rigged &stood) {
  drive = DriveState();
  drive.Rig = stood.Rig;
  auto &body = drive.Body;
  body.MassKg = stood.Envelope.MassKg;
  for (int axis = 0; axis < 3; ++axis) { body.InertiaKgM2[axis] = car.InertiaKgM2[axis]; }
  Placed start;
  if (!way.Line.At(0.0, start)) { return false; }
  const Standing under0 = outshine::Stand(way.Line, start.EastM, start.NorthM, kEdgeM, 0.0, 50.0);
  const double perWheelN =
      car.MassKg * stood.Envelope.GravityMs2 / (double)car.Contacts.size();
  const double settledM = car.Contacts[0].ReachM - perWheelN / car.Contacts[0].StiffnessNPerM;
  body.PositionM[0] = start.EastM;
  body.PositionM[1] = under0.HeightM + settledM - stood.Rig.Mounts[0].AtM[1];
  body.PositionM[2] = -start.NorthM;
  const double up[3] = {under0.NormalM[0], under0.NormalM[1], -under0.NormalM[2]};
  const double aheadM[3] = {std::cos(start.HeadingRad), start.Slope, -std::sin(start.HeadingRad)};
  outshine::Physics::Lie(body, aheadM, up);
  const double aheadBody[3] = {0.0, 0.0, -1.0};
  double ahead[3];
  outshine::Physics::Turn(body.OrientationQ, aheadBody, ahead);
  for (int axis = 0; axis < 3; ++axis) { body.VelocityMs[axis] = kJoinMs * ahead[axis]; }
  drive.CarWidthM = car.WidthM;
  drive.AsideRatePerM = (kLaneHalfM - 0.5 * car.WidthM) / stood.Envelope.TopMs();
  return true;
}

Ridden RideOut(const Corridor &way, const Rigged &stood, const Vehicle &car, DriveState &drive) {
  Ridden last;
  for (long step = 0; step < 4000; ++step) {
    last = DriveTick(way, stood, drive, kDtS, nullptr);
    if (!last.Found || last.Lost || last.OffTheRoad || last.PastLimit || last.Arrived) { break; }
  }
  return last;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const Vehicle car = Plausible();
  std::string error;

  const Rigged onEarth = Stand(car, kEarthMs2, 1.225);
  CHECK(onEarth.Stood, "the declaration stands as a rig under the declared 9.80665 m/s2");
  Corridor way;
  const bool laid = Straight(way, onEarth, kEdgeM, error);
  if (!laid) { std::printf("REFUSED %s\n", error.c_str()); }
  CHECK(laid, "a synthetic straight corridor lays in code -- line, plan and widths, no tile");

  DriveState drive;
  CHECK(Seat(drive, way, car, onEarth), "the car seats on the corridor's own start");
  const Ridden earth = RideOut(way, onEarth, car, drive);
  Note("how far the earth ride reached", earth.ReachedM, "m");
  Note("its worst offset past the settle-in", std::fabs(earth.WorstOffsetM), "m");
  Note("wheels airborne at worst", (double)earth.MostAirborne, "wheels");
  CHECK(earth.Arrived && !earth.OffTheRoad && !earth.Lost,
        "**THE TICK DRIVES THE CAR TO ARRIVAL ON FOUR FOUND CONTACTS.** One function, one "
        "corridor, one rig -- the whole drive loop the journeys delegate to, in the fast gate");
  CHECK(earth.MostAirborne < onEarth.Rig.Count,
        "**AND THE DECLARED GRAVITY POINTS DOWN**: a flipped sign in the tick's gravity vector "
        "would throw every wheel off the ground within a step, so a grounded arrival IS the "
        "sign's regression net");

  const Rigged onMoon = Stand(car, kMoonMs2, 0.0);
  Corridor moonWay;
  CHECK(Straight(moonWay, onMoon, kEdgeM, error) && onMoon.Stood,
        "the same corridor plans under a declared 1.62 m/s2 IN VACUUM -- 0 kg/m3 is legal air");
  DriveState moonDrive;
  CHECK(Seat(moonDrive, moonWay, car, onMoon), "and the same car seats on it");
  const Ridden moon = RideOut(moonWay, onMoon, car, moonDrive);
  Note("how far the moon ride reached", moon.ReachedM, "m");
  Note("moon wheels airborne at worst", (double)moon.MostAirborne, "wheels");
  Note("moon past a contact limit", moon.PastLimit ? 1.0 : 0.0, "flag");
  Note("moon lost the corridor", moon.Lost ? 1.0 : 0.0, "flag");
  CHECK(moon.Arrived && !moon.OffTheRoad,
        "**A SIXTH OF THE GRAVITY AND NO AIR AT ALL STILL HOLD THE CAR TO THE ROAD** -- less "
        "grip, no drag, the same arrival, and nothing in the tick names a planet");

  Corridor kerb;
  CHECK(Straight(kerb, onEarth, 0.5, error), "a corridor whose edge is inside the car's track lays");
  DriveState kerbed;
  CHECK(Seat(kerbed, kerb, car, onEarth), "the car seats on it");
  const Ridden off = RideOut(kerb, onEarth, car, kerbed);
  CHECK(off.OffTheRoad && off.LeftTheRoadAtM >= 0.0,
        "**A WHEEL PAST THE DECLARED EDGE IS A LOUD VERDICT, NOT A QUIET DRIVE** -- OffTheRoad "
        "reports the station, the offset and the plan the moment the surface ends");

  Covers("II.13 the drive tick is one pure function of (corridor, rig, vehicle, state): it "
         "holds the car to the declared world's gravity, arrives on a synthetic corridor in "
         "the fast gate, and reports the road's end loudly -- the regression net for the "
         "sign, the seat and the edge");
  return Report();
}
