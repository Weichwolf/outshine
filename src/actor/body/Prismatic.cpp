#include "Prismatic.h"

#include <algorithm>

namespace outshine::Physics {

Reaction Press(const PrismaticJoint &joint, Approach under) {
  Reaction out;
  out.ClosingMs = under.ClosingMs;
  out.PressedM = joint.ReachM - under.ClearanceM;
  if (!(out.PressedM > 0.0)) {
    out.PressedM = 0.0;
    return out;
  }
  out.Touching = true;

  double within = out.PressedM;
  double beyond = 0.0;
  if (joint.TravelM > 0.0 && within > joint.TravelM) {
    beyond = within - joint.TravelM;
    within = joint.TravelM;
    out.PastTravel = true;
  }
  out.ElasticN = joint.StiffnessNPerM * within;
  out.StopN = joint.StopStiffnessNPerM * beyond;
  out.DampingN = joint.DampingNsPerM * under.ClosingMs;

  out.LoadN = out.ElasticN + out.StopN + out.DampingN;
  out.LoadN = std::max(out.LoadN, 0.0);
  out.PastLimit = joint.LoadLimitN > 0.0 && out.LoadN > joint.LoadLimitN;
  return out;
}

double PressedForM(const PrismaticJoint &joint, double loadN) {
  if (!(joint.StiffnessNPerM > 0.0) || !(loadN > 0.0)) { return 0.0; }
  const double elastic = loadN / joint.StiffnessNPerM;
  if (joint.TravelM <= 0.0 || elastic <= joint.TravelM) { return elastic; }
  if (!(joint.StopStiffnessNPerM > 0.0)) { return joint.TravelM; }
  return joint.TravelM + (loadN - joint.StiffnessNPerM * joint.TravelM) / joint.StopStiffnessNPerM;
}

}
