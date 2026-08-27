#include <cmath>
#include <cstdio>

#include <Scenario.h>

#include "Check.h"
#include "Rigging.h"

namespace {

// THE ORACLE IS GEOMETRY AND IT OWES NOTHING TO OUR DESIGN: the distance between a body's front and
// rear contacts IS its wheelbase, and the distance between its left and right contacts IS its
// track. Neither is an opinion, so neither is something a declaration gets to state.
//
// `include/Scenario.h` stated both anyway -- `wheelbaseM` and `trackM` beside the four contacts
// that already fixed them. `Rigging` derived the wheelbase from the contacts, derived the track
// when it was not declared, and REFUSED when a declared wheelbase disagreed with the geometry by
// more than a millimetre. A guarded second spelling is still a second spelling: the guard is the
// mitigation, and the fix is that the number has one place to come from.
//
// Both fields are gone. What proves it is not that the drive still drives -- it does, to the digit,
// 10.5115 / 522.756 / 5.31713 before and after -- but that the derived span TRACKS ITS CONTACTS. A
// declared constant cannot do that: move an axle and a stated wheelbase stays where it was stated,
// while a derived one follows. So the case moves the axles and reads the span back.
//
// The steering lock is the reason this matters rather than a tidiness argument. It is
// `atan(wheelbase / (turningCircle/2 - track/2))`, so a wheelbase that disagrees with the contacts
// steers a car that is not the one standing on the road, and the physics reads the contacts.
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

[[nodiscard]] outshine::Body Spanning(double halfSpanM, double halfTrackM) {
  outshine::Body made;
  made.Name = "spanned";
  made.MassKg = 1610.0;
  made.WidthM = 1.811;
  made.CentreOfMassM[1] = 0.55;
  made.InertiaKgM2[0] = 540.0;
  made.InertiaKgM2[1] = 2400.0;
  made.InertiaKgM2[2] = 2600.0;
  made.Contacts.push_back(Standing(-halfTrackM, -halfSpanM));
  made.Contacts.push_back(Standing(halfTrackM, -halfSpanM));
  made.Contacts.push_back(Standing(-halfTrackM, halfSpanM));
  made.Contacts.push_back(Standing(halfTrackM, halfSpanM));
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

  const outshine::Sim::Rigged asF31 =
      outshine::Sim::Stand(Spanning(1.405, 0.774), kGravityMs2, kAirDensityKgM3);
  const outshine::Sim::Rigged longer =
      outshine::Sim::Stand(Spanning(1.605, 0.774), kGravityMs2, kAirDensityKgM3);
  const outshine::Sim::Rigged wider =
      outshine::Sim::Stand(Spanning(1.405, 0.974), kGravityMs2, kAirDensityKgM3);

  CHECK(asF31.Stood && longer.Stood && wider.Stood,
        "all three rigs stand, so the spans below are read from rigs and not from refusals");
  if (!(asF31.Stood && longer.Stood && wider.Stood)) { return Report(); }

  std::printf("CONTACTS AT z +-1.405   wheelbase %.4f m   steer lock %.5f rad\n",
              asF31.Axles.WheelbaseM, asF31.Axles.SteerLimitRad);
  std::printf("MOVED TO   z +-1.605    wheelbase %.4f m   steer lock %.5f rad\n",
              longer.Axles.WheelbaseM, longer.Axles.SteerLimitRad);
  std::printf("TRACK WIDENED TO +-0.974                   steer lock %.5f rad\n",
              wider.Axles.SteerLimitRad);

  CHECK(std::fabs(asF31.Axles.WheelbaseM - 2.810) < 1.0e-9,
        "**THE CONTACTS ARE THE WHEELBASE**: axles at z = -1.405 and +1.405 stand 2.810 m apart, "
        "and that is the number, not a `wheelbaseM` beside them saying the same thing");
  CHECK(std::fabs(longer.Axles.WheelbaseM - 3.210) < 1.0e-9,
        "and moving the axles MOVES it, which is the whole difference between a derived number "
        "and a declared one: a stated wheelbase stays where it was stated while the car it "
        "describes has changed shape");
  // WRITTEN THE WRONG WAY ROUND FIRST, and the measurement corrected it rather than the reverse.
  // The case expected a longer car to need LESS lock inside the same circle. It needs MORE: for a
  // path of radius R the steering angle is about L/R, so lengthening the wheelbase at a fixed
  // circle raises the angle. 0.52280 rad became 0.58221 rad, and the closed form the rig uses --
  // `atan(wheelbase / (circle/2 - track/2))` -- says so plainly with the wheelbase in the
  // numerator. The prediction was mine and the geometry was the code's.
  CHECK(longer.Axles.SteerLimitRad > asF31.Axles.SteerLimitRad,
        "and the steering lock follows, because it IS `atan(wheelbase / (circle/2 - track/2))` -- "
        "a longer car held to the same circle needs MORE lock, so a wheelbase that disagreed with "
        "the contacts would steer a car that is not the one standing on the road");
  CHECK(wider.Axles.SteerLimitRad > asF31.Axles.SteerLimitRad,
        "and the TRACK is read from the contacts too: a wider car has less room between its "
        "outer circle and its own width, so the same wheelbase needs more lock");

  Covers("the sim: a body's wheelbase and track are the distances between its contacts and are "
         "derived from them, so neither is declared and neither can disagree with the geometry "
         "the physics reads");
  return Report();
}
