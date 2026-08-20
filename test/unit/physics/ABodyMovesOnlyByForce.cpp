#include <cmath>
#include <cstdio>

#include "Check.h"

#include "Body.h"

using outshine::Physics::Body;
using outshine::Physics::Carry;
using outshine::Physics::EnergyJ;
using outshine::Physics::Fall;
using outshine::Physics::Push;
using outshine::Physics::Step;
using outshine::Physics::Wrench;

namespace {

constexpr double kGravity[3] = {0.0, -9.80665, 0.0};
constexpr double kMassKg = 1610.0;
constexpr double kRollKgM2 = 540.0;
constexpr double kPitchKgM2 = 2400.0;
constexpr double kYawKgM2 = 2600.0;

Body Car(void) {
  Body body;
  body.MassKg = kMassKg;
  body.InertiaKgM2[0] = kRollKgM2;
  body.InertiaKgM2[1] = kPitchKgM2;
  body.InertiaKgM2[2] = kYawKgM2;
  return body;
}

double FellIn(double dtS, long steps) {
  Body body = Car();
  for (long step = 0; step < steps; ++step) {
    Wrench wrench;
    Fall(wrench, body, kGravity);
    Step(body, wrench, dtS);
  }
  return -body.PositionM[1];
}

double MomentumOf(const Body &body) {
  return std::sqrt(std::pow(kRollKgM2 * body.SpinBodyRadS[0], 2.0) +
                   std::pow(kPitchKgM2 * body.SpinBodyRadS[1], 2.0) +
                   std::pow(kYawKgM2 * body.SpinBodyRadS[2], 2.0));
}

struct Tumble {
  double GrowthPerS = 0.0;
  double FlippedAtS = 0.0;
  double WorstMomentum = 0.0;
  double WorstEnergy = 0.0;
  bool Flipped = false;
};

Tumble Tumbled(double dtS, double forS) {
  Body body = Car();
  body.SpinBodyRadS[0] = 1.0e-6;
  body.SpinBodyRadS[1] = 10.0;

  const double momentumStart = MomentumOf(body);
  const double energyStart = EnergyJ(body, kGravity);
  const long steps = (long)std::llround(forS / dtS);

  Tumble out;
  double atHalf = 0.0, atOneAndAHalf = 0.0;
  for (long step = 0; step < steps; ++step) {
    const Wrench nothing;
    Step(body, nothing, dtS);
    const double atS = (double)(step + 1) * dtS;
    if (std::fabs(atS - 0.5) < 0.5 * dtS) { atHalf = std::fabs(body.SpinBodyRadS[0]); }
    if (std::fabs(atS - 1.5) < 0.5 * dtS) { atOneAndAHalf = std::fabs(body.SpinBodyRadS[0]); }
    if (!out.Flipped && body.SpinBodyRadS[1] < 0.0) {
      out.Flipped = true;
      out.FlippedAtS = atS;
    }
    out.WorstMomentum = std::fmax(out.WorstMomentum, std::fabs(MomentumOf(body) / momentumStart - 1.0));
    out.WorstEnergy =
        std::fmax(out.WorstEnergy, std::fabs(EnergyJ(body, kGravity) / energyStart - 1.0));
  }
  out.GrowthPerS = std::log(atOneAndAHalf / atHalf);
  return out;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const double forS = 1.0;
  const double exactM = 0.5 * 9.80665 * forS * forS;
  const double coarseM = FellIn(1.0e-3, 1000);
  const double fineM = FellIn(0.5e-3, 2000);
  Note("how far a body falls in one second", exactM, "m");
  Note("how far this integrator says it falls at 1 ms", coarseM, "m");
  Note("its error at 1 ms", coarseM - exactM, "m");
  Note("its error at 0.5 ms", fineM - exactM, "m");
  Note("the error the integrator's own leading term predicts at 1 ms",
       0.5 * 9.80665 * 1.0e-3 * forS, "m");

  CHECK_NEAR(coarseM - exactM, 0.5 * 9.80665 * 1.0e-3 * forS, 1.0e-5, "m",
             "**THE INTEGRATOR'S ERROR IS DERIVED AND NOT DISCOVERED.** A semi-implicit step "
             "advances the velocity before the position, so after n steps it has fallen g dt^2 "
             "n(n+1)/2 rather than g t^2 / 2 -- an excess of exactly g dt t / 2, which is what is "
             "measured here. An integrator whose error you can write down is an integrator whose "
             "step you can choose");
  CHECK_NEAR((coarseM - exactM) / (fineM - exactM), 2.0, 0.02, "x",
             "and halving the step halves the error, which is what FIRST ORDER means -- so the cost "
             "of an accuracy is a number rather than an experiment");

  const Tumble coarse = Tumbled(1.0e-4, 4.0);
  const Tumble fine = Tumbled(0.5e-4, 4.0);

  const double growth = 10.0 * std::sqrt((kPitchKgM2 - kRollKgM2) * (kYawKgM2 - kPitchKgM2) /
                                         (kRollKgM2 * kYawKgM2));
  Note("the rate at which a wobble about the intermediate axis grows", growth, "1/s");
  Note("the rate the integrator produces", coarse.GrowthPerS, "1/s");
  Note("when the spin about the intermediate axis reversed", coarse.FlippedAtS, "s");
  Note("worst drift in angular momentum over 4 s at 0.1 ms", coarse.WorstMomentum, "of it");
  Note("the same at 0.05 ms", fine.WorstMomentum, "of it");
  Note("worst drift in energy over 4 s at 0.1 ms", coarse.WorstEnergy, "of it");
  Note("the same at 0.05 ms", fine.WorstEnergy, "of it");

  CHECK(coarse.Flipped, "**A BODY SPUN ABOUT ITS INTERMEDIATE AXIS FLIPS OVER, and this one does.** "
                        "That is the tennis racket theorem, and it falls out of Euler's equations "
                        "rather than being put in -- so an integrator that produces it is carrying "
                        "the gyroscopic term, and one that does not has quietly dropped "
                        "omega x I omega");
  CHECK_NEAR(coarse.GrowthPerS, growth, 0.05, "1/s",
             "and the wobble grows at the rate the inertia tensor predicts, which is the "
             "QUANTITATIVE half: the flip could be an artefact, the rate cannot");
  CHECK_NEAR(coarse.WorstMomentum / fine.WorstMomentum, 2.0, 0.1, "x",
             "**THE ANGULAR MOMENTUM DRIFTS AND THE DRIFT IS THE STEP.** Nothing pushed this body, "
             "so every part of that drift is the integrator; halving the step halves it, which is "
             "what says so. A bound picked by hand would have hidden which of the two it was");
  CHECK_NEAR(coarse.WorstEnergy / fine.WorstEnergy, 2.0, 0.1, "x",
             "and the rotational energy drifts at the same order, so the flip is a rearrangement "
             "between axes and not an integrator doing work");
  CHECK(coarse.WorstMomentum < 1.0e-3 && coarse.WorstEnergy < 2.0e-3,
        "and at the step this suite runs, both stay under a tenth of a percent across a manoeuvre "
        "that moves the whole spin between two axes -- which is the number to quote, rather than a "
        "threshold that would have made the scaling above unnecessary to know");

  Body pushed;
  pushed.MassKg = kMassKg;
  pushed.InertiaKgM2[0] = kRollKgM2;
  pushed.InertiaKgM2[1] = kPitchKgM2;
  pushed.InertiaKgM2[2] = kYawKgM2;
  const double atM[3] = {0.0, 0.0, -1.405};
  const double sidewaysN[3] = {2000.0, 0.0, 0.0};
  Wrench shove;
  Push(shove, pushed, atM, sidewaysN);
  Note("the yaw torque a 2000 N shove on the front axle makes", shove.TorqueNm[1], "Nm");
  CHECK_NEAR(shove.TorqueNm[1], -1.405 * 2000.0, 1e-9, "Nm",
             "**A FORCE SOMEWHERE IS A FORCE AND A TORQUE**: r x F, with the arm turned into the "
             "world first. This is the only way anything in this engine ever rotates -- there is no "
             "second entrance where a heading is set. It is NEGATIVE because the front of a glTF "
             "car is -Z and up is +Y: shoving the nose towards +X turns the car about -Y, and a "
             "sign convention nobody wrote down is a crash waiting for the first steering input");
  CHECK(std::fabs(shove.TorqueNm[0]) < 1e-12 && std::fabs(shove.TorqueNm[2]) < 1e-12,
        "and a sideways shove on the centreline makes no roll and no pitch, because the arm and the "
        "force share a plane");

  Body carried;
  carried.MassKg = kMassKg;
  carried.VelocityMs[2] = 30.0;
  carried.SpinBodyRadS[1] = 0.5;
  double atCornerMs[3];
  const double cornerM[3] = {0.774, 0.0, 1.405};
  Carry(carried, cornerM, atCornerMs);
  Note("how fast the rear-right corner travels along the car while it yaws", atCornerMs[2], "m/s");
  Note("and how fast it travels sideways", atCornerMs[0], "m/s");
  CHECK_NEAR(atCornerMs[2], 30.0 - 0.5 * 0.774, 1e-9, "m/s",
             "**A POINT ON A BODY MOVES DIFFERENTLY FROM THE BODY.** v + omega x r is what a contact "
             "must be asked about its closing speed, or a car that is yawing damps as though it "
             "were not. Yawing about +Y carries the RIGHT rear corner backwards, which is the same "
             "convention the torque above obeys");
  CHECK_NEAR(atCornerMs[0], 0.5 * 1.405, 1e-9, "m/s",
             "and swings it outwards at the yaw rate times its distance from the axis, which is the "
             "half a tyre answers with a slip angle");

  Covers("I.9.3 a rigid body moves only by force: a first-order step whose error is derivable, "
         "Euler's equations complete enough to produce the tennis racket theorem at the rate the "
         "inertia predicts, torque from r x F alone, and the velocity of a point on it");
  return Report();
}
