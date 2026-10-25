#ifndef OUTSHINE_SIMULATION_PHYSICS_PRISMATICJOINTRESPONSE_H
#define OUTSHINE_SIMULATION_PHYSICS_PRISMATICJOINTRESPONSE_H

#include <PrismaticJoint.h>

namespace outshine::Physics {

struct Reaction {
  bool Touching = false;
  double PressedM = 0.0;
  double ClosingMs = 0.0;
  double LoadN = 0.0;
  double ElasticN = 0.0;
  double DampingN = 0.0;
  double StopN = 0.0;
  bool PastTravel = false;
  bool PastLimit = false;
};

struct Approach {
  double ClearanceM = 0.0;
  double ClosingMs = 0.0;
};

[[nodiscard]] Reaction Press(const PrismaticJoint &joint, Approach under);

[[nodiscard]] double PressedForM(const PrismaticJoint &joint, double loadN);

}

#endif
