#ifndef OUTSHINE_ACTOR_PATH_SPEEDPROFILE_H
#define OUTSHINE_ACTOR_PATH_SPEEDPROFILE_H

#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "ReferenceLine.h"

namespace outshine {

struct Envelope {
  double Grip = 0.0;
  double GravityMs2 = 0.0;
  double MassKg = 0.0;
  double DriveN = 0.0;
  double BrakeN = 0.0;
  double DragArea = 0.0;
  double AirDensity = 0.0;
  double ReserveMs2 = 0.0;
  double HoldWithinM = 0.0;
  double SettleS = 0.0;
  double CorneringNPerRad = 0.0;

  [[nodiscard]] double LateralMs2() const { return Grip * GravityMs2; }
  [[nodiscard]] double HoldingMs2() const {
    const double left = Grip * GravityMs2 - ReserveMs2;
    return left > 0.0 ? left : 0.0;
  }
  [[nodiscard]] double AccelMs2() const { return MassKg > 0.0 ? DriveN / MassKg : 0.0; }
  [[nodiscard]] double BrakeMs2() const {
    const double fromTyres = Grip * GravityMs2;
    const double fromBrakes = MassKg > 0.0 ? BrakeN / MassKg : 0.0;
    return fromBrakes < fromTyres ? fromBrakes : fromTyres;
  }
  [[nodiscard]] double TopMs() const {
    const double resistance = 0.5 * AirDensity * DragArea;
    return resistance > 0.0 ? std::sqrt(DriveN / resistance)
                            : std::numeric_limits<double>::infinity();
  }
};

class SpeedProfile {
public:
  [[nodiscard]] bool Over(const ReferenceLine &along, const Envelope &within, double stepM,
                          double entryMs, std::string &error);

  [[nodiscard]] double At(double alongM) const;
  [[nodiscard]] double StepM() const { return StepM_; }
  [[nodiscard]] size_t SampleCount() const { return Held_.size(); }
  [[nodiscard]] double SampleAt(size_t which) const { return Held_[which]; }
  [[nodiscard]] double CurvatureAt(size_t which) const { return Curvature_[which]; }
  [[nodiscard]] double CrestHeldMs() const { return CrestHeld_; }
  [[nodiscard]] double CrestHeldAtM() const { return CrestHeldAt_; }
  [[nodiscard]] size_t CrestsThatBound() const { return CrestsBound_; }

private:
  std::vector<double> Held_;
  std::vector<double> Curvature_;
  double StepM_ = 0.0;
  double LengthM_ = 0.0;
  double CrestHeld_ = 0.0;
  double CrestHeldAt_ = 0.0;
  size_t CrestsBound_ = 0;
};

}

#endif
