#ifndef OUTSHINE_STRUT_H
#define OUTSHINE_STRUT_H

namespace outshine {

struct Strut {
  double RestLengthM = 0.0;
  double WheelRadiusM = 0.0;
  double SpringNPerM = 0.0;
  double DamperNsPerM = 0.0;
  double TravelM = 0.0;
  double BumpStopNPerM = 0.0;
  double LinkLimitN = 0.0;
};

struct Touch {
  bool OnGround = false;
  double CompressionM = 0.0;
  double ClosingMs = 0.0;
  double LoadN = 0.0;
  double SpringN = 0.0;
  double DamperN = 0.0;
  double BumpStopN = 0.0;
  bool PastTravel = false;
  bool PastLink = false;
};

[[nodiscard]] Touch Press(const Strut &strut, double clearanceM, double closingMs);

[[nodiscard]] double SagM(const Strut &strut, double loadN);

} // namespace outshine

#endif
