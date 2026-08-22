#include <cmath>
#include <cstdio>

#include "Check.h"

#include "Rig.h"

using outshine::Physics::Bear;
using outshine::Physics::Body;
using outshine::Physics::Controls;
using outshine::Physics::Fall;
using outshine::Physics::Footing;
using outshine::Physics::Mount;
using outshine::Physics::Reading;
using outshine::Physics::Resist;
using outshine::Physics::Rig;
using outshine::Physics::Step;
using outshine::Physics::Wrench;

namespace {

constexpr double kGravity[3] = {0.0, -9.80665, 0.0};
constexpr double kMassKg = 1610.0;
constexpr double kCentreM = 0.55;
constexpr double kAnchorM = 0.333;
constexpr double kHalfTrackM = 0.774;
constexpr double kHalfBaseM = 1.405;
constexpr double kWheelbaseM = 2.810;
constexpr double kDragCoefficient = 0.66;
constexpr double kFrontalM2 = 2.19;
constexpr double kDragArea = kDragCoefficient * kFrontalM2;
constexpr double kAirDensity = 1.225;
constexpr double kDriveN = 400.0 * 3.08 / 0.333;
constexpr size_t kCorners = 4;

Rig F31(void) {
  Rig out;
  out.Count = kCorners;
  const double atM[kCorners][3] = {{-kHalfTrackM, kAnchorM - kCentreM, -kHalfBaseM},
                                   {kHalfTrackM, kAnchorM - kCentreM, -kHalfBaseM},
                                   {-kHalfTrackM, kAnchorM - kCentreM, kHalfBaseM},
                                   {kHalfTrackM, kAnchorM - kCentreM, kHalfBaseM}};
  const double reachM[kCorners] = {0.45635, 0.45635, 0.44909, 0.44909};
  const double stiffness[kCorners] = {32000.0, 32000.0, 34000.0, 34000.0};
  const double damping[kCorners] = {3400.0, 3400.0, 3600.0, 3600.0};
  for (size_t which = 0; which < kCorners; ++which) {
    Mount &mount = out.Mounts[which];
    for (int axis = 0; axis < 3; ++axis) { mount.AtM[axis] = atM[which][axis]; }
    mount.Touches.ReachM = reachM[which];
    mount.Touches.StiffnessNPerM = stiffness[which];
    mount.Touches.DampingNsPerM = damping[which];
    mount.Touches.TravelM = 0.18;
    mount.Touches.StopNPerM = 450000.0;
    mount.Touches.LimitN = 24000.0;
    mount.Sheds.StiffnessNPerRad = 55000.0;
    mount.Sheds.RelaxationM = 0.4;
    mount.Sheds.Friction = 0.95;
    mount.SteeredShare = atM[which][2] < 0.0 ? 1.0 : 0.0;
    mount.DrivenShare = atM[which][2] > 0.0 ? 0.5 : 0.0;
    mount.BrakedShare = 0.25;
  }
  return out;
}

Body Car(double atHeightM, double atSpeedMs) {
  Body body;
  body.MassKg = kMassKg;
  body.InertiaKgM2[0] = 540.0;
  body.InertiaKgM2[1] = 2400.0;
  body.InertiaKgM2[2] = 2600.0;
  body.PositionM[1] = atHeightM;
  body.VelocityMs[2] = -atSpeedMs;
  return body;
}

struct Ran {
  Reading Last;
  double HeightM = 0.0;
  double PitchRad = 0.0;
  double SpeedMs = 0.0;
  double SlowedMs2 = 0.0;
  double FrontN = 0.0;
  double RearN = 0.0;
};

Ran Ran_(Rig &rig, Body &body, const Controls &with, bool onGround, bool withDrag, double dtS,
         double forS) {
  Footing under[kCorners];
  for (size_t which = 0; which < kCorners; ++which) { under[which].Found = onGround; }

  const long steps = (long)std::llround(forS / dtS);
  const double wasMs = std::fabs(body.VelocityMs[2]);
  Ran out;
  for (long step = 0; step < steps; ++step) {
    Wrench wrench;
    Fall(wrench, body, kGravity);
    if (withDrag) { Resist(wrench, body, kDragArea, kAirDensity); }
    out.Last = Bear(rig, body, under, with, wrench, dtS);
    Step(body, wrench, dtS);
  }
  out.HeightM = body.PositionM[1];
  out.PitchRad = 2.0 * std::asin(body.OrientationQ[1]);
  out.SpeedMs = std::fabs(body.VelocityMs[2]);
  out.SlowedMs2 = forS > 0.0 ? (wasMs - out.SpeedMs) / forS : 0.0;
  out.FrontN = out.Last.LoadN[0] + out.Last.LoadN[1];
  out.RearN = out.Last.LoadN[2] + out.Last.LoadN[3];
  return out;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  Rig rig = F31();
  Body body = Car(kCentreM + 0.05, 0.0);
  const Controls idle;
  const Ran rest = Ran_(rig, body, idle, true, false, 1.0e-3, 8.0);

  Note("where the rig came to rest", rest.HeightM, "m");
  Note("where its centre of mass declares it stands", kCentreM, "m");
  Note("what its four mounts carry", rest.FrontN + rest.RearN, "N");
  Note("what it weighs", kMassKg * 9.80665, "N");
  Note("what the front axle carries", rest.FrontN, "N");
  Note("what the rear axle carries", rest.RearN, "N");

  CHECK_NEAR(rest.HeightM, kCentreM, 1.0e-4, "m",
             "**A RIG IS A BODY AND 1..N MOUNTS, AND IT STANDS WHERE THEY PUT IT.** Nothing in it "
             "is a wheel: a mount is an attachment point, a contact and a slip, plus what share of "
             "the steering, the drive and the braking it takes -- so a car has four, a motorbike "
             "two, a walker two and an aircraft three, and none of them is a different type");
  CHECK_NEAR(rest.FrontN + rest.RearN, kMassKg * 9.80665, 1.0, "N",
             "carrying its weight between them");
  CHECK_NEAR(rest.FrontN, rest.RearN, 1.0, "N",
             "**AND FRONT AND REAR CARRY THE SAME, WHICH NOBODY SET.** The centre of mass is midway "
             "between the axles, so 50:50 is where the arithmetic lands rather than something the "
             "springs were tuned to produce");
  CHECK(std::fabs(rest.PitchRad) < 1.0e-4, "standing level");
  CHECK(rest.Last.Airborne == 0 && !rest.Last.PastLimit,
        "with every mount on the ground and none of them complaining");

  const double slowingMs2 = 0.4 * 9.80665;
  Controls braking;
  braking.BrakeN = kMassKg * slowingMs2;
  Rig braked = F31();
  Body rolling = Car(kCentreM, 25.0);
  const Ran hard = Ran_(braked, rolling, braking, true, false, 1.0e-3, 2.0);

  const double transferN = 0.5 * (hard.FrontN - hard.RearN);
  const double predictedN = kMassKg * hard.SlowedMs2 * kCentreM / kWheelbaseM;
  Note("how hard it actually slowed", hard.SlowedMs2, "m/s2");
  Note("what the front axle carries braking", hard.FrontN, "N");
  Note("what the rear axle carries braking", hard.RearN, "N");
  Note("half the difference, which is the transfer", transferN, "N");
  Note("what m a h / L predicts", predictedN, "N");
  Note("how far apart those are", transferN / predictedN - 1.0, "of it");
  Note("the pitch it braked at", hard.PitchRad, "rad");

  CHECK_NEAR(hard.SlowedMs2, slowingMs2, 0.05, "m/s2",
             "asked to slow at 0.4 g through its mounts, it slows at 0.4 g -- so the shear the "
             "contacts shed IS the deceleration and there is no second path where a speed is set");
  CHECK_NEAR(transferN, predictedN, 0.05 * predictedN, "N",
             "**AND THE WEIGHT IT MOVES ONTO THE FRONT IS m a h / L, WHICH IS WHY THE FORCE MUST BE "
             "APPLIED AT THE CONTACT PATCH.** The patch is a reach below the mount, and pushing at "
             "the mount instead would have shortened the lever from 0.55 m to 0.217 m and lost 60 % "
             "of the transfer -- with the car still settling at the right height, still level, and "
             "every static check still green. That is the shape of defect this number exists to "
             "catch");
  CHECK(hard.PitchRad != 0.0 && rest.Last.Airborne == 0,
        "and it dives while it does it, because a transfer is a torque and a torque is what a body "
        "answers to");

  Rig flying = F31();
  Body falling = Car(kCentreM, 25.0);
  const Ran gone = Ran_(flying, falling, idle, false, false, 1.0e-3, 0.5);
  Note("mounts off the ground", (double)gone.Last.Airborne, "of 4");
  Note("how far it fell in half a second", kCentreM - gone.HeightM, "m");
  CHECK(gone.Last.Airborne == kCorners,
        "**TAKE THE GROUND AWAY AND IT SAYS SO, ON EVERY MOUNT.** A hole in a road is not detected "
        "-- it is READ, as four mounts that stopped touching anything");
  CHECK_NEAR(kCentreM - gone.HeightM, 0.5 * 9.80665 * 0.25, 0.01, "m",
             "and it falls at g while that lasts, which is the only thing that can happen to a body "
             "nothing is pushing");

  const double terminalMs = std::sqrt(kDriveN / (0.5 * kAirDensity * kDragArea));
  Rig cruising = F31();
  Body fast = Car(kCentreM, terminalMs);
  Controls flat;
  flat.DriveN = kDriveN;
  const Ran held = Ran_(cruising, fast, flat, true, true, 1.0e-3, 2.0);
  Note("the top speed sqrt(F / (rho A / 2)) predicts", terminalMs, "m/s");
  Note("the same in km/h", terminalMs * 3.6, "km/h");
  Note("what it was doing after two seconds at full drive", held.SpeedMs, "m/s");
  CHECK_NEAR(held.SpeedMs, terminalMs, 0.1, "m/s",
             "**AND AT THE TOP SPEED THE SPEED PROFILE PREDICTS, IT NEITHER GAINS NOR LOSES.** The "
             "planner solves sqrt(F / (rho A / 2)) from the declaration; the physics pushes with "
             "the same force against the same drag and finds the same balance. Neither was told "
             "about the other, and that is the second place in this engine where the two halves "
             "meet on a number");

  Covers("I.9.8 a rig is a body and 1..N mounts, each an attachment, a contact and a slip with a "
         "declared share of the steering, the drive and the braking: it stands where its mass says, "
         "transfers m a h / L onto the front under braking because the force acts at the contact "
         "patch, reads its own mounts leaving the ground, and balances at the top speed the "
         "planner derives");
  return Report();
}
