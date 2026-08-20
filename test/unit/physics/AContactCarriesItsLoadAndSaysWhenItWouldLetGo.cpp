#include <cmath>
#include <cstdio>

#include "Check.h"

#include "Contact.h"

using outshine::Physics::Contact;
using outshine::Physics::PressedForM;
using outshine::Physics::Press;
using outshine::Physics::Reaction;

namespace {

constexpr double kGravityMs2 = 9.80665;
constexpr double kMassKg = 1610.0;
constexpr double kCornerN = kMassKg * kGravityMs2 / 4.0;

Contact Declared() {
  Contact out;
  out.ReachM = 0.633;
  out.StiffnessNPerM = 32000.0;
  out.DampingNsPerM = 3400.0;
  out.TravelM = 0.18;
  out.StopNPerM = 450000.0;
  out.LimitN = 24000.0;
  return out;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const Contact contact = Declared();

  const Reaction air = Press(contact, contact.ReachM + 0.05, 0.0);
  CHECK(!air.Touching && air.LoadN == 0.0,
        "**FREE FALL IS A LOAD OF ZERO.** A contact that reaches nothing carries nothing, and that is "
        "not a crash condition somebody wrote -- it is the reading");

  const double pressed = PressedForM(contact, kCornerN);
  const Reaction resting = Press(contact, contact.ReachM - pressed, 0.0);
  Note("how far a corner of 1610 kg presses it", pressed, "m");
  Note("the load it carries there", resting.LoadN, "N");
  Note("the corner's weight", kCornerN, "N");
  CHECK_NEAR(resting.LoadN, kCornerN, 1e-9, "N",
             "**THE BODY STANDS EXACTLY WHERE ITS WEIGHT PUTS IT.** Pressed by mg/4k it carries mg/4 "
             "-- an identity, so the model is arithmetic rather than a fit");
  CHECK(pressed < contact.TravelM,
        "and it uses less than its travel standing still, or the body rests on its stop");

  const Reaction closing = Press(contact, contact.ReachM - pressed, 0.5);
  CHECK_NEAR(closing.LoadN - resting.LoadN, contact.DampingNsPerM * 0.5, 1e-9, "N",
             "a contact closing on what it touches carries more, by exactly the damping times the "
             "closing speed");

  const Reaction opening = Press(contact, contact.ReachM - pressed, -100.0);
  CHECK(opening.LoadN == 0.0,
        "**AND A CONTACT PUSHES BUT NEVER PULLS.** One opening faster than its stiffness can follow "
        "carries nothing rather than a negative load -- nothing here holds the ground up");

  const Reaction onStop = Press(contact, contact.ReachM - (contact.TravelM + 0.01), 0.0);
  CHECK(onStop.PastTravel, "past its travel a contact is on its stop");
  CHECK_NEAR(onStop.ElasticN, contact.StiffnessNPerM * contact.TravelM, 1e-9, "N",
             "where the elastic stage contributes what its whole travel is worth and no more");
  CHECK_NEAR(onStop.StopN, contact.StopNPerM * 0.01, 1e-9, "N",
             "and the stop carries the excess at its own far higher rate");

  double letsGoAtM = 0.0;
  for (double over = 0.0; over < 0.20; over += 0.00001) {
    if (Press(contact, contact.ReachM - (contact.TravelM + over), 0.0).PastLimit) {
      letsGoAtM = contact.TravelM + over;
      break;
    }
  }
  Note("how far it is pressed when its limit is exceeded", letsGoAtM, "m");
  CHECK(letsGoAtM > contact.TravelM,
        "**WHAT BREAKS IT IS DERIVED AND NOT DECLARED.** It is where the load exceeds the limit, "
        "which follows from the stiffness, the travel, the stop and the limit -- four physical "
        "quantities, none of them a threshold");
  const double predicted =
      (contact.LimitN - contact.StiffnessNPerM * contact.TravelM) / contact.StopNPerM;
  CHECK_NEAR(letsGoAtM - contact.TravelM, predicted, 2e-5, "m",
             "and the derivation is checkable in closed form: the excess over the travel is what the "
             "stop needs to make up the difference between the elastic contribution and the limit");

  const double closingForLimitMs = contact.LimitN / contact.DampingNsPerM;
  CHECK(Press(contact, contact.ReachM - 0.001, closingForLimitMs).PastLimit,
        "**AND SPEED REACHES THE LIMIT WITHOUT TRAVEL.** A contact meeting something fast enough goes "
        "on the DAMPING before it has moved -- which is why a defect's severity depends on how fast "
        "the body was going and not only on how large the defect was");
  Note("closing speed that reaches the limit on damping alone", closingForLimitMs, "m/s");

  Covers("I.9.1 a compliant contact is a reach, a stiffness, a damping, a travel, a stop and a "
         "limit: it carries its share of the weight by identity, never pulls, and what breaks it is "
         "derived from those six. It is not a strut, a wheel, a landing gear or a leg -- the physics "
         "knows none of those, and what a contact IS belongs to the declaration that placed it");
  return Report();
}
