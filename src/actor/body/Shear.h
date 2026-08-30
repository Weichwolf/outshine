#ifndef OUTSHINE_ACTOR_BODY_SHEAR_H
#define OUTSHINE_ACTOR_BODY_SHEAR_H

namespace outshine::Physics {

struct Shearing {
  double CorneringNPerRad = 0.0;
  double AlongStiffnessN = 0.0;
  double RelaxationM = 0.0;
  double Grip = 0.0;
  double FrictionAtLoadN = 0.0;
  double LoadFalloff = 0.0;
};

struct Shear {
  double AcrossN = 0.0;
  double AlongN = 0.0;
  double AngleRad = 0.0;
  double Ratio = 0.0;
  double HoldN = 0.0;
  bool Sliding = false;
};

[[nodiscard]] Shear
ShedAt(const Shearing &through, double loadN, double slipRad, double askedAlongN);

[[nodiscard]] Shear
Shed(const Shearing &through, double loadN, double acrossMs, double alongMs, double askedAlongN);

[[nodiscard]] double Relaxed(const Shearing &through, double wasRad, double isRad, double rolledM);

[[nodiscard]] double FrictionAt(const Shearing &through, double loadN);

[[nodiscard]] double Brushed(double linearN, double holdN);

} // namespace outshine::Physics

#endif
