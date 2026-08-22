#ifndef OUTSHINE_PHYSICS_CONTACT_H
#define OUTSHINE_PHYSICS_CONTACT_H

namespace outshine::Physics {

struct Contact {
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

[[nodiscard]] Reaction Press(const Contact &contact, double clearanceM, double closingMs);

[[nodiscard]] double PressedForM(const Contact &contact, double loadN);

} // namespace outshine::Physics

#endif
