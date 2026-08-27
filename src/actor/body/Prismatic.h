#ifndef OUTSHINE_ACTOR_BODY_PRISMATIC_H
#define OUTSHINE_ACTOR_BODY_PRISMATIC_H

namespace outshine::Physics {

struct Prismatic {
  double ReachM = 0.0;
  double StiffnessNPerM = 0.0;
  double DampingNsPerM = 0.0;
  double TravelM = 0.0;
  double StopNPerM = 0.0;
  double LimitN = 0.0;
};

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

[[nodiscard]] Reaction Press(const Prismatic &joint, double clearanceM, double closingMs);

[[nodiscard]] double PressedForM(const Prismatic &joint, double loadN);

}

#endif
