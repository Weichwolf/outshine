#include <cmath>
#include <cstdio>

#include <Scenario.h>

#include "Check.h"
#include "Rigging.h"

namespace {

// TARGET NAMES ACTUATORS AND THE TREE HAD NUMBERS: *ACTUATORS -- the functions a body declares:
// steer, drive, brake, lamps, walk, open*. `include/Scenario.h` carried `PeakTorqueNm`,
// `FinalDrive`, `BrakeTorqueNm` and `TurningCircleM` as fields of a vehicle, and
// `src/engine/Assembly.cpp:200` INFERRED what the body could do from whether they were non-zero:
//
//     const bool steers = vehicle.TurningCircleM > 0.0;
//     const bool drives = vehicle.PeakTorqueNm > 0.0 && vehicle.FinalDrive > 0.0;
//     const bool brakes = vehicle.BrakeTorqueNm > 0.0;
//
// That is a capability guessed from a magnitude, and the guess and the truth come apart at exactly
// one place: a body that HAS a drive and can currently deliver no torque. A dead engine, a
// disconnected motor, a drivetrain the scenario means to build up. The old shape said such a body
// has no drive at all; it has one, and it is producing nothing.
//
// The oracle is which of the two a declaration is FOR. A declaration states what a thing IS, and
// the engine reads it where the decision is made -- CLAUDE.md's own rule about `Declared` flags,
// applied to a body's functions. So drive, brake and steer are a declared catalogue now, the
// numbers are each actuator's strength, and the reader refuses a fourth name rather than ignoring
// it.
//
// The drive is unchanged: 10.5115 / 522.756 / 5.31713, the same digits as before the wheelbase
// deletion, before the tyre moved, and before this.
constexpr double kGravityMs2 = 9.80665;
constexpr double kAirDensityKgM3 = 1.225;

[[nodiscard]] outshine::Contact Standing(double xM, double zM) {
  outshine::Contact one;
  one.At = zM < 0.0 ? "front" : "rear";
  one.AtM[0] = xM;
  one.AtM[1] = 0.333;
  one.AtM[2] = zM;
  one.Strut.ReachM = 0.45635;
  one.Strut.StiffnessNPerM = 32000.0;
  one.Strut.DampingNsPerM = 3400.0;
  one.Strut.TravelM = 0.18;
  one.Strut.StopNPerM = 450000.0;
  one.Strut.LimitN = 24000.0;
  one.Touches.Grip = 0.95;
  one.Touches.RadiusM = 0.333;
  one.Touches.CorneringNPerRad = 55000.0;
  one.Touches.RelaxationM = 0.4;
  return one;
}

[[nodiscard]] outshine::Body Doing(double driveNm) {
  outshine::Body made;
  made.Name = "declared";
  made.MassKg = 1610.0;
  made.WidthM = 1.811;
  made.CentreOfMassM[1] = 0.55;
  made.InertiaKgM2[0] = 540.0;
  made.InertiaKgM2[1] = 2400.0;
  made.InertiaKgM2[2] = 2600.0;
  made.Contacts.push_back(Standing(-0.774, -1.405));
  made.Contacts.push_back(Standing(0.774, -1.405));
  made.Contacts.push_back(Standing(-0.774, 1.405));
  made.Contacts.push_back(Standing(0.774, 1.405));
  made.Driven.push_back(outshine::Drive{outshine::Drives::Effort, false, driveNm, 3.08, 0.0});
  made.Driven.push_back(outshine::Drive{outshine::Drives::Effort, true, 5500.0, 1.0, 0.0});
  made.Driven.push_back(outshine::Drive{outshine::Drives::Motion, false, 0.0, 1.0, 11.3});
  made.DragCoefficient = 0.66;
  made.FrontalM2 = 2.19;
  return made;
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const outshine::Body running = Doing(400.0);
  const outshine::Body stalled = Doing(0.0);

  outshine::Body towed = Doing(400.0);
  towed.Driven.erase(towed.Driven.begin());

  const outshine::Sim::Rigged asRuns =
      outshine::Sim::Stand(running, kGravityMs2, kAirDensityKgM3);
  const outshine::Sim::Rigged asStalls =
      outshine::Sim::Stand(stalled, kGravityMs2, kAirDensityKgM3);
  const outshine::Sim::Rigged asTowed = outshine::Sim::Stand(towed, kGravityMs2, kAirDensityKgM3);

  CHECK(asRuns.Stood && asStalls.Stood && asTowed.Stood,
        "all three rigs stand -- a body with no drive is a rig and not a refusal, which is the "
        "point: a trailer is a body too");
  if (!(asRuns.Stood && asStalls.Stood && asTowed.Stood)) { return Report(); }

  std::printf("DECLARES DRIVE 400 Nm   can drive %s   drive force %9.2f N\n",
              running.Efforts(false) != nullptr ? "yes" : "no ",
              asRuns.Envelope.DriveN);
  std::printf("DECLARES DRIVE   0 Nm   can drive %s   drive force %9.2f N\n",
              stalled.Efforts(false) != nullptr ? "yes" : "no ",
              asStalls.Envelope.DriveN);
  std::printf("DECLARES NO DRIVE       can drive %s   drive force %9.2f N\n",
              towed.Efforts(false) != nullptr ? "yes" : "no ",
              asTowed.Envelope.DriveN);

  CHECK(asRuns.Envelope.DriveN > 0.0,
        "a body that declares a drive and states a torque produces a force, so the two readings "
        "below are distinctions and not an envelope that is empty for everyone");
  CHECK(stalled.Efforts(false) != nullptr && asStalls.Envelope.DriveN == 0.0,
        "**A BODY CAN HAVE A DRIVE AND PRODUCE NOTHING**: a dead engine is a body WITH a drive "
        "delivering no torque, and the old shape -- `drives = PeakTorqueNm > 0 && FinalDrive > "
        "0` -- called it a body with no drive at all. A capability guessed from a magnitude is "
        "wrong exactly here, and this is the case that separates them");
  CHECK(towed.Efforts(false) == nullptr && asTowed.Envelope.DriveN == 0.0,
        "and a body that declares NO drive has none -- a trailer, which is the other half of the "
        "distinction and stands as a rig rather than being refused for lacking an engine");
  CHECK(asRuns.Axles.SteerLimitRad > 0.0 && asRuns.Envelope.BrakeN > 0.0,
        "and steer and brake are declared the same way, so the three functions TARGET names come "
        "from one catalogue rather than from three unrelated fields");

  Covers("the sim: what a body can do is what it DECLARES -- drive, brake and steer are actuators "
         "from a catalogue and the numbers are their strength, so a drive delivering no torque is "
         "still a drive");
  return Report();
}
