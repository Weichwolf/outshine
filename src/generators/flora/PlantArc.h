#ifndef OUTSHINE_GENERATORS_FLORA_PLANTARC_H
#define OUTSHINE_GENERATORS_FLORA_PLANTARC_H

#include "math/Vec3.h"
#include <cmath>

namespace outshine::Generators {
struct PlantArcFrame {
  Vec3 Centre, Tangent, Lateral, Normal;
};

struct PlantArc {
  double LengthM = 0, BendRad = 0, TwistRad = 0;

  [[nodiscard]] PlantArcFrame FrameAt(double parameter) const noexcept {
    const double angle = BendRad * parameter;
    const double roll = TwistRad * parameter;
    const double half = Sinc(angle / 2);
    PlantArcFrame result;
    result.Centre = {{0,
                      LengthM * parameter * Sinc(angle),
                      LengthM * BendRad * parameter * parameter / 2 * half * half}};
    result.Tangent = {{0, std::cos(angle), std::sin(angle)}};
    result.Lateral = {
        {std::cos(roll), -std::sin(roll) * std::sin(angle), std::sin(roll) * std::cos(angle)}};
    result.Normal = Cross(result.Lateral, result.Tangent);
    return result;
  }

private:
  static double Sinc(double angle) noexcept { return angle == 0 ? 1.0 : std::sin(angle) / angle; }
};
}
#endif
