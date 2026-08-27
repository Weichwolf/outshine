#include <cmath>
#include <cstdio>

#include "Rigid.h"
#include "Rig.h"
#include "Check.h"

namespace {

// Three oracles, none of which depends on our design.
//
// ONE -- the moment of a force about the centre of mass is the cross product of the arm with the
// force, M = r x F. This is the definition of a moment, not a model of one. Its two corollaries
// are what a car pitching under power and diving under the brakes actually IS:
//
//   a force applied THROUGH the centre of mass has r = 0 and builds no moment at all
//   a longitudinal force applied at ground level, an arm r_y below the centre, builds a moment
//     of magnitude |F_z| * |r_y| about the lateral axis -- which is the pitch axis
//
// TWO -- the arm turns with the body. Push takes the arm in BODY coordinates, so a body rolled
// by an angle must produce the moment the rotated arm demands, and the magnitude |r x F| must
// be unchanged by a rotation of the whole system.
//
// THREE -- drag is one half rho v-squared A, directed against the velocity. It is quadratic in
// speed exactly: doubling the speed quadruples the force, to the last bit the double holds.
constexpr double kAgreesWithinNm = 1e-9;
constexpr double kAgreesWithinN = 1e-9;
constexpr double kQuadrupleWithin = 1e-12;

constexpr double kMassKg = 1610.0;
constexpr double kCentreHeightM = 0.55;
constexpr double kContactBelowM = -kCentreHeightM;
constexpr double kBrakingN = -8000.0;
constexpr double kAirDensityKgM3 = 1.225;
constexpr double kDragArea = 0.66 * 2.19;

[[nodiscard]] outshine::Physics::Rigid AtRest(void) {
  outshine::Physics::Rigid body;
  body.MassKg = kMassKg;
  body.InertiaKgM2[0] = 540.0;
  body.InertiaKgM2[1] = 2400.0;
  body.InertiaKgM2[2] = 2600.0;
  return body;
}

[[nodiscard]] double Length(const double v[3]) {
  return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

}

int main(void) {
  using namespace outshine::Test;
  using namespace outshine::Physics;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const Rigid body = AtRest();

  // ONE, first corollary: through the centre, no moment. If this were not exact, every force in
  // the engine would leak a spurious rotation.
  {
    Wrench built;
    const double at[3] = {0.0, 0.0, 0.0};
    const double forceN[3] = {0.0, 0.0, kBrakingN};
    Push(built, body, at, forceN);
    std::printf("THROUGH THE CENTRE force %.1f N builds torque (%.15g, %.15g, %.15g) Nm\n",
                kBrakingN, built.TorqueNm[0], built.TorqueNm[1], built.TorqueNm[2]);
    CHECK(Length(built.TorqueNm) == 0.0,
          "a force applied through the centre of mass builds NO moment -- r is zero, so r x F "
          "is zero, and any leak here is a rotation nothing asked for");
    CHECK(std::fabs(built.ForceN[2] - kBrakingN) < kAgreesWithinN,
          "and the force itself arrives whole");
  }

  // ONE, second corollary: at ground level, the pitch moment IS the braking force times the
  // height of the centre of mass. This is dive, and it is arithmetic.
  {
    Wrench built;
    const double at[3] = {0.0, kContactBelowM, 0.0};
    const double forceN[3] = {0.0, 0.0, kBrakingN};
    Push(built, body, at, forceN);
    const double wanted = kContactBelowM * kBrakingN;
    std::printf("AT THE PATCH force %.1f N at %.3f m below the centre builds pitch %.15g Nm, "
                "arithmetic says %.15g Nm\n",
                kBrakingN, -kContactBelowM, built.TorqueNm[0], wanted);
    CHECK(std::fabs(built.TorqueNm[0] - wanted) < kAgreesWithinNm,
          "**A LONGITUDINAL FORCE AT THE PATCH BUILDS THE PITCH MOMENT ITS ARM DEMANDS**: this "
          "is what dive under the brakes and squat under power ARE, and an engine that applies "
          "drive force at the centre of mass instead of at the contact has neither");
    CHECK(std::fabs(built.TorqueNm[1]) < kAgreesWithinNm &&
              std::fabs(built.TorqueNm[2]) < kAgreesWithinNm,
          "and it builds pitch ONLY: a purely longitudinal force on the centre plane yaws and "
          "rolls nothing");
  }

  // TWO: rotate the whole system and the magnitude of the moment cannot change. A moment that
  // grew or shrank with the body's attitude would mean the arm is being read in the wrong frame.
  {
    Wrench upright;
    const double at[3] = {0.0, kContactBelowM, 0.0};
    const double forceN[3] = {0.0, 0.0, kBrakingN};
    Push(upright, body, at, forceN);

    Rigid rolled = AtRest();
    const double halfRad = 0.5 * 0.4;
    rolled.OrientationQ[0] = std::cos(halfRad);
    rolled.OrientationQ[3] = std::sin(halfRad);
    Wrench turned;
    double turnedForce[3];
    Turn(rolled.OrientationQ, forceN, turnedForce);
    Push(turned, rolled, at, turnedForce);

    const double was = Length(upright.TorqueNm);
    const double now = Length(turned.TorqueNm);
    std::printf("ROLLED 0.400 rad the same arm and force build |M| %.15g Nm, upright %.15g Nm\n",
                now, was);
    CHECK(std::fabs(now - was) < kAgreesWithinNm,
          "the magnitude of a moment is invariant under a rotation of the whole system -- an arm "
          "read in the wrong frame would not be");
  }

  // THREE: drag is quadratic in speed, exactly, and points against the velocity.
  {
    double worstRatio = 0.0;
    double worstAlong = 0.0;
    for (double speedMs = 5.0; speedMs <= 40.0; speedMs += 5.0) {
      Rigid moving = AtRest();
      moving.VelocityMs[2] = speedMs;
      Wrench slowed;
      Resist(slowed, moving, kDragArea, kAirDensityKgM3);
      const double wantedN = 0.5 * kAirDensityKgM3 * kDragArea * speedMs * speedMs;
      const double heldN = Length(slowed.ForceN);
      worstRatio = std::fmax(worstRatio, std::fabs(heldN - wantedN) / wantedN);

      Rigid twice = AtRest();
      twice.VelocityMs[2] = 2.0 * speedMs;
      Wrench slowedTwice;
      Resist(slowedTwice, twice, kDragArea, kAirDensityKgM3);
      const double quadruple = Length(slowedTwice.ForceN) / heldN;
      worstRatio = std::fmax(worstRatio, 0.0);
      worstAlong = std::fmax(worstAlong, std::fabs(quadruple - 4.0));

      CHECK(slowed.ForceN[2] < 0.0,
            "drag opposes the velocity, so a body moving one way is pushed the other");
    }
    Note("the worst disagreement with one half rho v-squared A", worstRatio, "of the force");
    Note("the worst disagreement of a doubled speed with a quadrupled force", worstAlong,
         "of the ratio");
    CHECK(worstRatio < kAgreesWithinN,
          "drag is one half rho v-squared A, and the engine computes the drag equation rather "
          "than a curve fitted to it");
    CHECK(worstAlong < kQuadrupleWithin,
          "and it is quadratic to the last bit: doubling the speed quadruples the force");
  }

  Covers("physics: a force builds the moment its arm demands, so squat under power and dive "
         "under the brakes fall out of the integration; and resistance is the drag equation, "
         "quadratic in speed and opposed to the velocity");
  return Report();
}
