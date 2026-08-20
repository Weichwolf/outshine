#include <cmath>
#include <cstdio>

#include "Check.h"

#include "Body.h"
#include "Contact.h"

using outshine::Physics::Body;
using outshine::Physics::Carry;
using outshine::Physics::Contact;
using outshine::Physics::Fall;
using outshine::Physics::Place;
using outshine::Physics::Press;
using outshine::Physics::PressedForM;
using outshine::Physics::Push;
using outshine::Physics::Reaction;
using outshine::Physics::Step;
using outshine::Physics::Wrench;

namespace {

constexpr double kGravity[3] = {0.0, -9.80665, 0.0};
constexpr double kMassKg = 1610.0;
constexpr double kAnchorM = 0.333;
constexpr double kHalfTrackM = 0.774;
constexpr double kHalfBaseM = 1.405;
constexpr size_t kCorners = 4;

struct Corner {
  double AtM[3];
  Contact Touches;
};

Corner kCar[kCorners] = {
    {{-kHalfTrackM, 0.0, -kHalfBaseM}, {0.45635, 32000.0, 3400.0, 0.18, 450000.0, 24000.0}},
    {{kHalfTrackM, 0.0, -kHalfBaseM}, {0.45635, 32000.0, 3400.0, 0.18, 450000.0, 24000.0}},
    {{-kHalfTrackM, 0.0, kHalfBaseM}, {0.44909, 34000.0, 3600.0, 0.18, 450000.0, 24000.0}},
    {{kHalfTrackM, 0.0, kHalfBaseM}, {0.44909, 34000.0, 3600.0, 0.18, 450000.0, 24000.0}},
};

Body Car(double atHeightM) {
  Body body;
  body.MassKg = kMassKg;
  body.InertiaKgM2[0] = 540.0;
  body.InertiaKgM2[1] = 2400.0;
  body.InertiaKgM2[2] = 2600.0;
  body.PositionM[1] = atHeightM;
  return body;
}

struct Rested {
  double HeightM = 0.0;
  double PitchRad = 0.0;
  double RollRad = 0.0;
  double LoadN[kCorners] = {0.0, 0.0, 0.0, 0.0};
  double HeaviestN = 0.0;
  bool PastTravel = false;
  bool PastLimit = false;
};

Rested Settled(double fromHeightM, double dtS, double forS) {
  Body body = Car(fromHeightM);
  Rested out;
  const long steps = (long)std::llround(forS / dtS);
  for (long step = 0; step < steps; ++step) {
    Wrench wrench;
    Fall(wrench, body, kGravity);
    for (size_t which = 0; which < kCorners; ++which) {
      double worldM[3], worldMs[3];
      Place(body, kCar[which].AtM, worldM);
      Carry(body, kCar[which].AtM, worldMs);
      const Reaction against = Press(kCar[which].Touches, worldM[1], -worldMs[1]);
      out.LoadN[which] = against.LoadN;
      out.HeaviestN = std::fmax(out.HeaviestN, against.LoadN);
      out.PastTravel = out.PastTravel || against.PastTravel;
      out.PastLimit = out.PastLimit || against.PastLimit;
      if (against.LoadN <= 0.0) { continue; }
      const double upwardN[3] = {0.0, against.LoadN, 0.0};
      Push(wrench, body, kCar[which].AtM, upwardN);
    }
    Step(body, wrench, dtS);
  }
  out.HeightM = body.PositionM[1];
  out.PitchRad = 2.0 * std::asin(body.OrientationQ[1]);
  out.RollRad = 2.0 * std::asin(body.OrientationQ[3]);
  return out;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const double cornerN = kMassKg * 9.80665 / 4.0;
  Note("what one corner of this car weighs", cornerN, "N");
  Note("how far that presses the front contact", PressedForM(kCar[0].Touches, cornerN), "m");
  Note("how far that presses the rear contact", PressedForM(kCar[2].Touches, cornerN), "m");

  const Rested rest = Settled(kAnchorM + 0.10, 1.0e-3, 8.0);
  Note("where the car came to rest", rest.HeightM, "m");
  Note("where its contacts say it stands", kAnchorM, "m");
  Note("what it left on the front-left contact", rest.LoadN[0], "N");
  Note("what it left on the rear-left contact", rest.LoadN[2], "N");
  Note("the pitch it came to rest at", rest.PitchRad, "rad");
  Note("the roll it came to rest at", rest.RollRad, "rad");

  CHECK_NEAR(rest.HeightM, kAnchorM, 1.0e-4, "m",
             "**A CAR DROPPED 100 MM ABOVE ITS OWN RIDE HEIGHT COMES BACK TO IT.** Nothing placed it "
             "there: it fell, four contacts pushed back, and the height it stopped at is where the "
             "reach and the stiffness put it. The declaration and the physics are the same "
             "statement seen from two sides");

  double totalN = 0.0;
  for (size_t which = 0; which < kCorners; ++which) { totalN += rest.LoadN[which]; }
  Note("what the four contacts carry between them", totalN, "N");
  Note("what the car weighs", kMassKg * 9.80665, "N");
  CHECK_NEAR(totalN, kMassKg * 9.80665, 1.0, "N",
             "and between them they carry exactly its weight, which is the whole of what standing "
             "still means");

  CHECK(std::fabs(rest.PitchRad) < 1.0e-4 && std::fabs(rest.RollRad) < 1.0e-4,
        "**AND IT STANDS LEVEL, WHICH NOBODY ARRANGED.** The front and rear rates differ by 2000 "
        "N/m; the reaches differ by 7.26 mm to match, and the car being level is what those four "
        "numbers agreeing LOOKS like. Declare the reaches equal and it stands nose-down");

  CHECK(!rest.PastTravel && !rest.PastLimit,
        "and a 100 mm drop uses neither the bump travel nor anything near the limit -- a car has to "
        "be treated badly before a contact complains");

  double lowM = 0.0, highM = 2.0;
  for (int narrow = 0; narrow < 24; ++narrow) {
    const double tryM = 0.5 * (lowM + highM);
    if (Settled(kAnchorM + tryM, 0.5e-3, 1.5).PastLimit) {
      highM = tryM;
    } else {
      lowM = tryM;
    }
  }
  Note("the drop that first breaks a contact", 0.5 * (lowM + highM), "m");
  Note("the speed it arrives at", std::sqrt(2.0 * 9.80665 * 0.5 * (lowM + highM)), "m/s");
  Note("the load its limit stands at", kCar[0].Touches.LimitN, "N");
  Note("how many times the corner's own weight that is", kCar[0].Touches.LimitN / cornerN, "x");

  CHECK(0.5 * (lowM + highM) > 0.05 && 0.5 * (lowM + highM) < 1.5,
        "**A CRASH IS READ AND NOT DETECTED.** Nobody wrote a rule about how far a car may fall: it "
        "falls, the contact is pressed past its travel into a stop 14 times stiffer, and somewhere "
        "the load crosses the limit the declaration gave it. That height is a CONSEQUENCE of six "
        "declared numbers and gravity, which is why the same mechanism answers for a kerb, a "
        "pothole, a bridge joint and a landing gear");

  const Rested broken = Settled(kAnchorM + 1.0, 0.5e-3, 1.5);
  Note("the heaviest load a 1 m drop puts through one contact", broken.HeaviestN, "N");
  CHECK(broken.PastLimit && broken.PastTravel,
        "and a metre says so in both ways it can, so the two declarations are separately readable: "
        "the travel ran out, and then the load went past what the link carries");

  Covers("I.9.4 a body standing on compliant contacts settles at the height its declaration "
         "implies, level, carrying its own weight -- and the drop that breaks a contact is derived "
         "from those numbers rather than being a rule somebody wrote about crashing");
  return Report();
}
