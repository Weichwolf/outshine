#include <cmath>
#include <cstdio>

#include <Scenario.h>

#include "Check.h"
#include "Rigging.h"

namespace {

// A TYRE IS A PROPERTY OF A CONTACT PATCH, NOT OF A CAR, and the oracle is that plain fact: two
// patches can carry different rubber, and any real vehicle where they do -- a staggered set, a
// space-saver spare, a worn axle -- is not expressible by a declaration that states one tyre for
// the whole body.
//
// `include/Scenario.h` stated one. `<tyre grip radiusM corneringNPerRad relaxationM/>` sat beside
// `<contact>` and applied to all of them, which is a vehicle NOUN in the door twice over: it names
// a car's part, and it assumes a car's symmetry.
//
// `Rigging` already disagreed with it. It copied those four numbers into `mount.Sheds` PER MOUNT
// (`Rigging.cpp:150-153`) -- the internal representation was contact-wise all along and only the
// declaration was not. So this moved the fields onto `Contact`, where the physics had already put
// them, and the reader takes them off `<contact>`.
//
// THE PROOF IS THE CAPABILITY THE MOVE BUYS. A body whose front and rear carry different rubber
// must rig with different friction at front and rear. One tyre per vehicle cannot express that at
// all, so a case that measures it fails against the old shape by construction rather than by
// tolerance. The drive is unchanged where the rubber is uniform: 10.5115 / 522.756 / 5.31713
// before and after.
constexpr double kGravityMs2 = 9.80665;
constexpr double kAirDensityKgM3 = 1.225;

[[nodiscard]] outshine::Contact Standing(double xM, double zM, double grip, double radiusM) {
  outshine::Contact one;
  one.At = zM < 0.0 ? "front" : "rear";
  one.AtM[0] = xM;
  one.AtM[1] = 0.333;
  one.AtM[2] = zM;
  one.ReachM = 0.45635;
  one.StiffnessNPerM = 32000.0;
  one.DampingNsPerM = 3400.0;
  one.TravelM = 0.18;
  one.StopNPerM = 450000.0;
  one.LimitN = 24000.0;
  one.Grip = grip;
  one.RadiusM = radiusM;
  one.CorneringNPerRad = 55000.0;
  one.RelaxationM = 0.4;
  return one;
}

[[nodiscard]] outshine::Body Wearing(double frontGrip, double rearGrip) {
  outshine::Body made;
  made.Name = "staggered";
  made.MassKg = 1610.0;
  made.WidthM = 1.811;
  made.CentreOfMassM[1] = 0.55;
  made.InertiaKgM2[0] = 540.0;
  made.InertiaKgM2[1] = 2400.0;
  made.InertiaKgM2[2] = 2600.0;
  made.Contacts.push_back(Standing(-0.774, -1.405, frontGrip, 0.333));
  made.Contacts.push_back(Standing(0.774, -1.405, frontGrip, 0.333));
  made.Contacts.push_back(Standing(-0.774, 1.405, rearGrip, 0.333));
  made.Contacts.push_back(Standing(0.774, 1.405, rearGrip, 0.333));
  made.Driven.push_back(outshine::Drive{outshine::Drives::Effort, false, 400.0, 3.08, 0.0});
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

  const outshine::Sim::Rigged same =
      outshine::Sim::Stand(Wearing(0.95, 0.95), kGravityMs2, kAirDensityKgM3);
  const outshine::Sim::Rigged staggered =
      outshine::Sim::Stand(Wearing(0.95, 0.55), kGravityMs2, kAirDensityKgM3);

  CHECK(same.Stood && staggered.Stood, "both rigs stand, so what follows is read from rigs");
  if (!(same.Stood && staggered.Stood)) { return Report(); }
  CHECK(same.Rig.Count == 4 && staggered.Rig.Count == 4,
        "and both stand on four mounts, so the mounts compared below are the same mounts");
  if (!(same.Rig.Count == 4 && staggered.Rig.Count == 4)) { return Report(); }

  const double sameFront = same.Rig.Mounts[0].Sheds.Friction;
  const double sameRear = same.Rig.Mounts[2].Sheds.Friction;
  const double onFront = staggered.Rig.Mounts[0].Sheds.Friction;
  const double onRear = staggered.Rig.Mounts[2].Sheds.Friction;

  std::printf("ONE RUBBER ALL ROUND   front %.4f  rear %.4f\n", sameFront, sameRear);
  std::printf("STAGGERED 0.95 / 0.55  front %.4f  rear %.4f\n", onFront, onRear);

  CHECK(std::fabs(sameFront - 0.95) < 1.0e-12 && std::fabs(sameRear - 0.95) < 1.0e-12,
        "the uniform body rigs the grip its contacts state, at both ends, so the staggered "
        "reading below is a difference and not an artefact of where the numbers land");
  CHECK(std::fabs(onFront - 0.95) < 1.0e-12 && std::fabs(onRear - 0.55) < 1.0e-12,
        "**A TYRE BELONGS TO ITS CONTACT PATCH**: front and rear rig with the rubber each one "
        "states. A declaration carrying ONE tyre for the whole body cannot express a staggered "
        "set, a space-saver or a worn axle at all -- and `Rigging` already copied those numbers "
        "per mount, so the physics had put them here before the declaration did");
  CHECK(onFront > onRear,
        "and the difference has the sign the declaration gave it, which is what makes this a "
        "measurement rather than two numbers that happen to differ");

  Covers("the sim: a tyre is declared on the contact patch it belongs to, so a body can carry "
         "different rubber at each contact and the rig reads what each one states");
  return Report();
}
