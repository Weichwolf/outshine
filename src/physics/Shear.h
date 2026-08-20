#ifndef OUTSHINE_PHYSICS_SHEAR_H
#define OUTSHINE_PHYSICS_SHEAR_H

namespace outshine::Physics {

struct Slip {
  double StiffnessNPerRad = 0.0;
  double AlongStiffnessN = 0.0;
  double RelaxationM = 0.0;
  double Friction = 0.0;
};

struct Shear {
  double AcrossN = 0.0;
  double AlongN = 0.0;
  double AngleRad = 0.0;
  double Ratio = 0.0;
  double HoldN = 0.0;
  bool Sliding = false;
};

[[nodiscard]] Shear Shed(const Slip &through, double loadN, double acrossMs, double alongMs,
                         double askedAlongN);

[[nodiscard]] double Relaxed(const Slip &through, double wasRad, double isRad, double rolledM);

} // namespace outshine::Physics

#endif
