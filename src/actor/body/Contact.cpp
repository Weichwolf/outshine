#include "Contact.h"

namespace outshine::Physics {

Reaction Press(const Contact &contact, double clearanceM, double closingMs) {
  Reaction out;
  out.ClosingMs = closingMs;
  out.PressedM = contact.ReachM - clearanceM;
  if (!(out.PressedM > 0.0)) {
    out.PressedM = 0.0;
    return out;
  }
  out.Touching = true;

  double within = out.PressedM;
  double beyond = 0.0;
  if (contact.TravelM > 0.0 && within > contact.TravelM) {
    beyond = within - contact.TravelM;
    within = contact.TravelM;
    out.PastTravel = true;
  }
  out.ElasticN = contact.StiffnessNPerM * within;
  out.StopN = contact.StopNPerM * beyond;
  out.DampingN = contact.DampingNsPerM * closingMs;

  out.LoadN = out.ElasticN + out.StopN + out.DampingN;
  if (out.LoadN < 0.0) { out.LoadN = 0.0; }
  out.PastLimit = contact.LimitN > 0.0 && out.LoadN > contact.LimitN;
  return out;
}

double PressedForM(const Contact &contact, double loadN) {
  if (!(contact.StiffnessNPerM > 0.0) || !(loadN > 0.0)) { return 0.0; }
  const double elastic = loadN / contact.StiffnessNPerM;
  if (contact.TravelM <= 0.0 || elastic <= contact.TravelM) { return elastic; }
  if (!(contact.StopNPerM > 0.0)) { return contact.TravelM; }
  return contact.TravelM + (loadN - contact.StiffnessNPerM * contact.TravelM) / contact.StopNPerM;
}

} // namespace outshine::Physics
