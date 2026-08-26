#include <cmath>
#include <cstdio>

#include <Scenario.h>

#include "Check.h"
#include "Rig.h"
#include "Rigging.h"

namespace {

// The oracle is static equilibrium, and it does not depend on our design: a body at rest on two
// axles carries a load on each that satisfies both sum(F) = 0 and sum(M) = 0 about any point.
// Taking moments about the rear contact line gives the front axle load directly:
//
//   N_front * (a + b) = m * g * b      ->      N_front / (m * g) = b / (a + b)
//
// where a is the distance from the centre of mass to the front axle and b to the rear. A brake
// system proportioned to that share reaches the friction limit at both axles together; one
// proportioned any other way reaches it at one axle first, and the axle that reaches it first
// is the axle that locks. This test computes the share to more digits than the rig holds and
// asks whether the rig agrees.
constexpr double kAgreesWithin = 1e-12;

constexpr double kMassKg = 1610.0;
constexpr double kGravityMs2 = 9.80665;
constexpr double kAirDensityKgM3 = 1.225;

[[nodiscard]] outshine::Contact Standing(double xM, double zM) {
  outshine::Contact one;
  one.At = zM < 0.0 ? "front" : "rear";
  one.AtM[0] = xM;
  one.AtM[1] = 0.333;
  one.AtM[2] = zM;
  one.ReachM = 0.45;
  one.StiffnessNPerM = 32000.0;
  one.DampingNsPerM = 3400.0;
  one.TravelM = 0.18;
  one.StopNPerM = 450000.0;
  one.LimitN = 24000.0;
  one.Grip = 0.95;
  one.RadiusM = 0.333;
  one.CorneringNPerRad = 55000.0;
  one.RelaxationM = 0.4;
  return one;
}

[[nodiscard]] outshine::Vehicle Declared(double centreZM, double frontZM, double rearZM) {
  outshine::Vehicle made;
  made.Name = "two-axle body";
  made.MassKg = kMassKg;
  made.WidthM = 1.811;
  made.CentreOfMassM[1] = 0.55;
  made.CentreOfMassM[2] = centreZM;
  made.InertiaKgM2[0] = 540.0;
  made.InertiaKgM2[1] = 2400.0;
  made.InertiaKgM2[2] = 2600.0;
  made.Contacts.push_back(Standing(-0.774, frontZM));
  made.Contacts.push_back(Standing(0.774, frontZM));
  made.Contacts.push_back(Standing(-0.774, rearZM));
  made.Contacts.push_back(Standing(0.774, rearZM));
  made.TurningCircleM = 11.3;
  made.PeakTorqueNm = 400.0;
  made.FinalDrive = 3.08;
  made.BrakeTorqueNm = 5500.0;
  made.DragCoefficient = 0.66;
  made.FrontalM2 = 2.19;
  made.AssetWheelbase = rearZM - frontZM;
  return made;
}

struct Split {
  bool Stood = false;
  double Front = 0.0;
  double Rear = 0.0;
};

[[nodiscard]] Split Proportioned(const outshine::Vehicle &declared) {
  const outshine::Sim::Rigged stood =
      outshine::Sim::Stand(declared, kGravityMs2, kAirDensityKgM3);
  Split out;
  out.Stood = stood.Stood;
  if (!stood.Stood) { return out; }
  for (size_t which = 0; which < stood.Rig.Count; ++which) {
    const outshine::Physics::Mount &mount = stood.Rig.Mounts[which];
    (mount.AtM[2] < 0.0 ? out.Front : out.Rear) += mount.BrakedShare;
  }
  return out;
}

}

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  // Three bodies on one wheelbase, differing only in where the mass sits: centred, forward,
  // and rearward. A share that is derived answers all three; a share that is assumed answers
  // the centred one by coincidence and the other two not at all.
  struct Case {
    const char *What;
    double CentreZM, FrontZM, RearZM;
  };
  const Case asked[] = {
      {"mass exactly between the axles", 0.0, -1.405, 1.405},
      {"mass 0.400 m forward of centre", -0.400, -1.405, 1.405},
      {"mass 0.300 m rearward of centre", 0.300, -1.405, 1.405},
  };

  double worst = 0.0;
  const char *worstAt = "";
  for (const Case &one : asked) {
    const double frontArmM = one.CentreZM - one.FrontZM;
    const double rearArmM = one.RearZM - one.CentreZM;
    const double wanted = rearArmM / (frontArmM + rearArmM);

    const Split held = Proportioned(Declared(one.CentreZM, one.FrontZM, one.RearZM));
    CHECK(held.Stood, "the declaration stands, so its proportioning can be read");
    if (!held.Stood) { continue; }

    std::printf("BODY %s: static front load share %.15f, brake share %.15f\n", one.What, wanted,
                held.Front);
    CHECK(std::fabs(held.Front + held.Rear - 1.0) < kAgreesWithin,
          "the shares sum to one, so the declared brake torque is spent and no more");
    const double off = std::fabs(held.Front - wanted);
    if (off > worst) {
      worst = off;
      worstAt = one.What;
    }
  }

  Note("the worst disagreement with static equilibrium", worst, "of the whole");
  Note("where that was", 0.0, worstAt[0] == 0 ? "nowhere" : worstAt);
  CHECK(worst < kAgreesWithin,
        "**THE BRAKE IS PROPORTIONED TO THE LOAD THE AXLE CARRIES**: a share taken from the "
        "count of contacts rather than from the static load reaches the friction limit at one "
        "axle before the other, and the axle that reaches it first is the axle that locks -- "
        "under braking that is the rear, and a rear that locks first is a spin");

  // The negative control this test exists to invert: an even split, which is what a share taken
  // from the contact COUNT gives. It answers the centred body and no other, so a corpus that
  // only ever asked about a centred body would have been satisfied by it.
  double worstEven = 0.0;
  for (const Case &one : asked) {
    const double frontArmM = one.CentreZM - one.FrontZM;
    const double rearArmM = one.RearZM - one.CentreZM;
    const double wanted = rearArmM / (frontArmM + rearArmM);
    const double even = 0.5;
    worstEven = std::fmax(worstEven, std::fabs(even - wanted));
  }
  Note("what an even split disagrees by, over the same three bodies", worstEven, "of the whole");
  CHECK(worstEven > 0.1,
        "and the control is a control: over these three bodies an even split is wrong by more "
        "than a tenth of the whole braking effort, so this test can tell the two apart");

  Covers("physics: a body's brake effort is proportioned to the static load each axle carries, "
         "derived from where the declaration puts the centre of mass and never from the number "
         "of contacts -- the engine reads bodies and forces, not axles it has heard of");
  return Report();
}
