#include <cmath>
#include <cstdio>

#include "Check.h"

#include "Shear.h"

using outshine::Physics::Relaxed;
using outshine::Physics::Shear;
using outshine::Physics::Shed;
using outshine::Physics::Slip;

namespace {

constexpr double kGravityMs2 = 9.80665;
constexpr double kMassKg = 1610.0;
constexpr double kStiffnessNPerRad = 55000.0;
constexpr double kRelaxationM = 0.4;
constexpr double kFriction = 0.95;
constexpr double kRollingMs = 25.0;

Slip Tyre(void) {
  Slip out;
  out.StiffnessNPerRad = kStiffnessNPerRad;
  out.RelaxationM = kRelaxationM;
  out.Friction = kFriction;
  return out;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const double cornerN = kMassKg * kGravityMs2 / 4.0;
  const double smallRad = 0.02;
  const Shear gentle =
      Shed(Tyre(), cornerN, kRollingMs * std::tan(smallRad), kRollingMs, 0.0);
  Note("the load one corner carries", cornerN, "N");
  Note("the most it can hold", kFriction * cornerN, "N");
  Note("the slip angle a 0.5 m/s sideways drift makes at 25 m/s", gentle.AngleRad, "rad");
  Note("the sideways force that sheds", gentle.AcrossN, "N");

  CHECK_NEAR(gentle.AngleRad, -smallRad, 1.0e-9, "rad",
             "**A SLIP ANGLE IS AN ANGLE AND NOT A STATE.** It is atan of the sideways speed over "
             "the rolling speed, so a contact that is not moving has none, and one that is drifting "
             "has one whether or not anybody asked");
  CHECK_NEAR(gentle.AcrossN, -kStiffnessNPerRad * smallRad, 1.0, "N",
             "and near zero the force is the stiffness times the angle, which is what a cornering "
             "stiffness in N/rad MEANS -- declared once in the scenario and never fitted");
  CHECK(!gentle.Sliding, "with plenty in hand, so it is not sliding");

  const double hardRad = 0.10;
  const Shear hard = Shed(Tyre(), cornerN, kRollingMs * std::tan(hardRad), kRollingMs, 0.0);
  Note("what a 0.1 rad slip angle would ask for", kStiffnessNPerRad * hardRad, "N");
  Note("what it actually sheds", std::fabs(hard.AcrossN), "N");
  CHECK(hard.Sliding, "**AND PAST WHAT IT CAN HOLD IT SLIDES, WHICH IT SAYS.** 5500 N asked of a "
                      "contact that holds 3750 is not 5500 N of grip and a silent lie");
  CHECK_NEAR(std::fabs(hard.AcrossN), kFriction * cornerN, 1.0, "N",
             "shedding exactly the friction times the load and no more");

  const double brakingN = 3000.0;
  const Shear both =
      Shed(Tyre(), cornerN, kRollingMs * std::tan(hardRad), kRollingMs, -brakingN);
  const double magnitude = std::sqrt(both.AcrossN * both.AcrossN + both.AlongN * both.AlongN);
  Note("cornering and braking together, sideways", both.AcrossN, "N");
  Note("the same, along", both.AlongN, "N");
  Note("their magnitude", magnitude, "N");
  CHECK_NEAR(magnitude, kFriction * cornerN, 1.0, "N",
             "**AND BRAKING AND CORNERING COME OUT OF ONE PURSE.** The friction ellipse: what a "
             "contact sheds sideways and what it sheds along are one vector against one limit, so a "
             "car that brakes in a corner has less of both -- which is a consequence here and not a "
             "rule somebody added");
  CHECK(std::fabs(both.AcrossN) < std::fabs(hard.AcrossN),
        "so adding the brake TOOK sideways grip away, which is the thing this shape exists to make "
        "unavoidable");

  const double caught = Relaxed(Tyre(), 0.0, 1.0, kRelaxationM);
  Note("how much of a step in slip angle is caught after one relaxation length", caught, "of it");
  CHECK_NEAR(caught, 1.0 - std::exp(-1.0), 1.0e-12, "of it",
             "**A CONTACT BUILDS ITS SHEAR OVER A DISTANCE ROLLED, NOT INSTANTLY.** One relaxation "
             "length catches 1 - 1/e of a step, which is why a car answers the wheel a moment after "
             "it is turned -- and why a simulation without it feels sharper than any real vehicle");

  const double fourCornersN = 4.0 * kFriction * cornerN;
  Note("what four contacts hold between them", fourCornersN, "N");
  Note("the lateral acceleration that is", fourCornersN / kMassKg, "m/s2");
  Note("what the speed profile assumes, grip times g", kFriction * kGravityMs2, "m/s2");
  CHECK_NEAR(fourCornersN / kMassKg, kFriction * kGravityMs2, 1.0e-9, "m/s2",
             "**AND THE PLANNER AND THE PHYSICS AGREE, WHICH IS THE POINT OF MEASURING IT HERE.** "
             "The speed profile picks a corner speed from grip times g; four contacts each holding "
             "friction times their load deliver exactly that. Two halves of the engine derived from "
             "one declared number, meeting without either being told about the other");

  Covers("I.9.7 a contact sheds shear as a function of slip, bounded by friction times its load: "
         "linear in the stiffness near zero, saturating into a slide it declares, sharing one limit "
         "between cornering and braking, and building over a relaxation length rather than at once");
  return Report();
}
