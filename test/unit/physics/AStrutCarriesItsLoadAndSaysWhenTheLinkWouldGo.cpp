#include <cmath>
#include <cstdio>

#include "Check.h"

#include "Strut.h"

using outshine::Press;
using outshine::SagM;
using outshine::Strut;
using outshine::Touch;

namespace {

constexpr double kGravityMs2 = 9.80665;
constexpr double kMassKg = 1610.0;
constexpr double kCornerN = kMassKg * kGravityMs2 / 4.0;

Strut Declared() {
  Strut out;
  out.RestLengthM = 0.30;
  out.WheelRadiusM = 0.333;
  out.SpringNPerM = 32000.0;
  out.DamperNsPerM = 3400.0;
  out.TravelM = 0.18;
  out.BumpStopNPerM = 450000.0;
  out.LinkLimitN = 24000.0;
  return out;
}

double Reach(const Strut &strut) { return strut.RestLengthM + strut.WheelRadiusM; }

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const Strut strut = Declared();

  const Touch air = Press(strut, Reach(strut) + 0.05, 0.0);
  CHECK(!air.OnGround && air.LoadN == 0.0,
        "**FREE FALL IS A NORMAL FORCE OF ZERO.** A wheel that reaches nothing carries nothing, and "
        "that is not a crash condition somebody wrote -- it is the reading");

  const double sag = SagM(strut, kCornerN);
  const Touch resting = Press(strut, Reach(strut) - sag, 0.0);
  Note("static sag at a corner of 1610 kg", sag, "m");
  Note("the load it carries there", resting.LoadN, "N");
  Note("the corner's weight", kCornerN, "N");
  CHECK_NEAR(resting.LoadN, kCornerN, 1e-9, "N",
             "**THE CAR STANDS UP EXACTLY WHERE ITS WEIGHT PUTS IT.** The sag is mg/4k and the force "
             "at that compression is mg/4 -- an identity, so the model is arithmetic rather than a "
             "fit");
  CHECK(sag < strut.TravelM,
        "and it uses less than its travel standing still, or the car is on its bump stops in the car "
        "park");

  const Touch settling = Press(strut, Reach(strut) - sag, 0.5);
  CHECK(settling.LoadN > resting.LoadN,
        "a strut closing on the ground carries more than one at rest, which is the damper");
  CHECK_NEAR(settling.LoadN - resting.LoadN, strut.DamperNsPerM * 0.5, 1e-9, "N",
             "by exactly the damper rate times the closing speed");

  const Touch rebounding = Press(strut, Reach(strut) - sag, -100.0);
  CHECK(rebounding.LoadN == 0.0,
        "**AND A SUSPENSION PUSHES BUT NEVER PULLS.** A strut extending faster than its spring can "
        "follow carries nothing rather than a negative load -- a wheel does not hold the road down");

  const Touch onStop = Press(strut, Reach(strut) - (strut.TravelM + 0.01), 0.0);
  CHECK(onStop.PastTravel, "past its travel the strut is on its bump stop");
  CHECK_NEAR(onStop.SpringN, strut.SpringNPerM * strut.TravelM, 1e-9, "N",
             "where the spring contributes what its whole travel is worth and no more");
  CHECK_NEAR(onStop.BumpStopN, strut.BumpStopNPerM * 0.01, 1e-9, "N",
             "and the bump stop carries the excess at its own far stiffer rate");

  double tearsAtM = 0.0;
  for (double over = 0.0; over < 0.20; over += 0.00001) {
    const Touch pressed = Press(strut, Reach(strut) - (strut.TravelM + over), 0.0);
    if (pressed.PastLink) {
      tearsAtM = strut.TravelM + over;
      break;
    }
  }
  Note("compression at which the link's limit is exceeded", tearsAtM, "m");
  CHECK(tearsAtM > strut.TravelM,
        "**THE BUMP THAT TEARS A WHEEL OFF IS DERIVED AND NOT DECLARED.** It is where the load "
        "exceeds the link's strength, which follows from the spring, the travel, the bump stop and "
        "the link -- four physical quantities, none of them a threshold");

  const double reserveM = tearsAtM - SagM(strut, kCornerN);
  Note("compression left between standing still and the link letting go", reserveM, "m");
  CHECK(reserveM > 0.05 && reserveM < 0.5,
        "and a car has some of it in hand, which is what a suspension is for");

  const double aboveTravel = tearsAtM - strut.TravelM;
  const double predicted = (strut.LinkLimitN - strut.SpringNPerM * strut.TravelM) / strut.BumpStopNPerM;
  CHECK_NEAR(aboveTravel, predicted, 2e-5, "m",
             "and the derivation is checkable in closed form: the excess over the travel is what the "
             "bump stop needs to make up the difference between the spring's contribution and the "
             "link's limit");

  const double closingForLimitMs = strut.LinkLimitN / strut.DamperNsPerM;
  const Touch struck = Press(strut, Reach(strut) - 0.001, closingForLimitMs);
  Note("closing speed that reaches the link on the damper alone", closingForLimitMs, "m/s");
  CHECK(struck.PastLink,
        "**AND SPEED REACHES THE LIMIT WITHOUT COMPRESSION.** A wheel meeting a step fast enough tears "
        "on the DAMPER before the spring has moved -- which is why a defect's severity depends on how "
        "fast the car was going and not only on how tall the step was");

  Covers("I.9.1 a suspension strut is a spring, a damper, a travel, a bump stop and a link strength: "
         "it carries mg/4 at rest by identity, never pulls, and the bump at which the link lets go is "
         "derived from those five rather than declared as a threshold");
  return Report();
}
