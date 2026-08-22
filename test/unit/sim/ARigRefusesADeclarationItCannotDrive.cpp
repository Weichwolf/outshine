#include <cstdio>

#include "Check.h"

#include "Rigging.h"

using outshine::Contact;
using outshine::Vehicle;
using outshine::Sim::Rigged;
using outshine::Sim::Stand;

namespace {

Vehicle Plausible() {
  Vehicle out;
  out.MassKg = 1000.0;
  out.WidthM = 1.8;
  out.WheelbaseM = 2.5;
  out.TurningCircleM = 11.0;
  out.Grip = 0.9;
  out.TyreRadiusM = 0.3;
  out.CorneringNPerRad = 50000.0;
  out.RelaxationM = 0.4;
  out.PeakTorqueNm = 300.0;
  out.FinalDrive = 3.0;
  out.BrakeTorqueNm = 5000.0;
  out.DragCoefficient = 0.3;
  out.FrontalM2 = 2.2;
  out.AirDensity = 1.225;
  out.CentreOfMassM[1] = 0.5;
  const double reach = 0.45;
  for (int corner = 0; corner < 4; ++corner) {
    Contact one;
    one.AtM[0] = corner % 2 == 0 ? -0.75 : 0.75;
    one.AtM[1] = 0.3;
    one.AtM[2] = corner < 2 ? -1.25 : 1.25;
    one.ReachM = reach;
    one.StiffnessNPerM = 30000.0;
    one.DampingNsPerM = 3000.0;
    one.TravelM = 0.18;
    one.StopNPerM = 400000.0;
    one.LimitN = 20000.0;
    out.Contacts.push_back(one);
  }
  return out;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  const Rigged good = Stand(Plausible(), 9.80665);
  if (!good.Stood) { std::printf("REFUSED %s\n", good.Error.c_str()); }
  CHECK(good.Stood, "a plausible four-contact declaration stands up");

  Vehicle noseHeavy = Plausible();
  for (Contact &one : noseHeavy.Contacts) { one.AtM[2] = -1.25; }
  const Rigged refused = Stand(noseHeavy, 9.80665);
  CHECK(!refused.Stood,
        "**A VEHICLE WITH NO CONTACT BEHIND THE CENTRE OF MASS REFUSES INSTEAD OF STANDING UP "
        "SILENTLY UNDRIVABLE**: the old fallback set every DrivenShare to zero and said "
        "nothing -- the refusal names the missing drive axle");
  CHECK(refused.Error.find("centre of mass") != std::string::npos,
        "and the refusal says why");

  const Rigged onTheMoon = Stand(Plausible(), 1.62);
  CHECK(onTheMoon.Stood && onTheMoon.Envelope.GravityMs2 == 1.62,
        "**THE RIG STANDS IN THE DECLARED WORLD'S GRAVITY, NOT IN A CONSTANT'S**: 1.62 m/s2 "
        "(measured, lunar surface mean) flows into the envelope every limit derives from");
  CHECK(!Stand(Plausible(), 0.0).Stood,
        "and a world declaring no gravity refuses -- nothing would hold a wheel to the ground");

  Vehicle onThePlane = Plausible();
  for (Contact &one : onThePlane.Contacts) { one.AtM[2] = 0.0; }
  CHECK(!Stand(onThePlane, 9.80665).Stood,
        "contacts exactly on the centre plane belong to no axle, and that degenerate "
        "declaration refuses by the same rule rather than falling through two strict "
        "predicates");

  Covers("II.12 the rig drives or refuses: the drive axle is the contacts behind the centre "
         "of mass, a missing one is a named refusal, and the dead steering counter is gone");
  return Report();
}
