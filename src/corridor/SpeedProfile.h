#ifndef OUTSHINE_SPEEDPROFILE_H
#define OUTSHINE_SPEEDPROFILE_H

#include <cmath>
#include <string>
#include <vector>

#include "ReferenceLine.h"

namespace outshine {

inline constexpr double kGravityMs2 = 9.80665;

struct Envelope {
  double Grip = 0.0;
  double MassKg = 0.0;
  double DriveN = 0.0;
  double BrakeN = 0.0;
  double DragArea = 0.0;
  double AirDensity = 0.0;
  double ReserveMs2 = 0.0;
  double HoldWithinM = 0.0;
  double SettleS = 0.0;

  [[nodiscard]] double LateralMs2(void) const { return Grip * kGravityMs2; }
  [[nodiscard]] double HoldingMs2(void) const {
    const double left = Grip * kGravityMs2 - ReserveMs2;
    return left > 0.0 ? left : 0.0;
  }
  [[nodiscard]] double AccelMs2(void) const { return MassKg > 0.0 ? DriveN / MassKg : 0.0; }
  [[nodiscard]] double BrakeMs2(void) const {
    const double fromTyres = Grip * kGravityMs2;
    const double fromBrakes = MassKg > 0.0 ? BrakeN / MassKg : 0.0;
    return fromBrakes < fromTyres ? fromBrakes : fromTyres;
  }
  [[nodiscard]] double TopMs(void) const {
    const double resistance = 0.5 * AirDensity * DragArea;
    return resistance > 0.0 ? std::sqrt(DriveN / resistance) : 0.0;
  }
};

class SpeedProfile {
public:
  [[nodiscard]] bool Over(const ReferenceLine &along, const Envelope &within, double stepM,
                          double entryMs, std::string &error);

  [[nodiscard]] double At(double alongM) const;
  [[nodiscard]] double StepM(void) const { return StepM_; }
  [[nodiscard]] size_t SampleCount(void) const { return Held_.size(); }
  [[nodiscard]] double SampleAt(size_t which) const { return Held_[which]; }
  [[nodiscard]] double CurvatureAt(size_t which) const { return Curvature_[which]; }
  [[nodiscard]] double CrestHeldMs(void) const { return CrestHeld_; }
  [[nodiscard]] double CrestHeldAtM(void) const { return CrestHeldAt_; }
  [[nodiscard]] size_t CrestsThatBound(void) const { return CrestsBound_; }

private:
  std::vector<double> Held_;
  std::vector<double> Curvature_;
  double StepM_ = 0.0;
  double LengthM_ = 0.0;
  double CrestHeld_ = 0.0;
  double CrestHeldAt_ = 0.0;
  size_t CrestsBound_ = 0;
};

} // namespace outshine

#endif
