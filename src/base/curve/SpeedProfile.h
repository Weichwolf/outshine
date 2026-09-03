#ifndef OUTSHINE_BASE_CURVE_SPEEDPROFILE_H
#define OUTSHINE_BASE_CURVE_SPEEDPROFILE_H

#include <array>
#include <cmath>
#include <cstdint>
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

struct Walking {
  double StepM = 0.0;
  double EntryMs = 0.0;
};

class SpeedProfile {
public:
  enum class Held : uint8_t {
    Free,
    Curvature,
    Slip,
    Ramp,
    Climb,
    Crest,
    Entry,
    Traction,
    Brake,
    kCount
  };

  struct Bound {
    double Ms = 0.0;
    double AtM = 0.0;
    Held By = Held::Free;
  };

  [[nodiscard]] bool
  Over(const ReferenceLine &along, const Envelope &within, Walking by, std::string &error);

  [[nodiscard]] Bound Slowest() const noexcept { return Slowest_; }

  [[nodiscard]] Bound Fastest() const noexcept { return Fastest_; }

  [[nodiscard]] size_t BoundBy(Held term) const noexcept {
    return static_cast<size_t>(term) < static_cast<size_t>(Held::kCount)
               ? Bound_[static_cast<size_t>(term)]
               : 0;
  }

  [[nodiscard]] static const char *NameOf(Held term) noexcept;

  [[nodiscard]] double At(double alongM) const;

  [[nodiscard]] double StepM() const noexcept { return StepM_; }

  [[nodiscard]] size_t SampleCount() const noexcept { return Held_.size(); }

  [[nodiscard]] double SampleAt(size_t which) const noexcept { return Held_[which]; }

  [[nodiscard]] double CurvatureAt(size_t which) const noexcept { return Curvature_[which]; }

  [[nodiscard]] double Quantile(double share) const noexcept;
  [[nodiscard]] size_t StationsUnder(double ms) const noexcept;

  [[nodiscard]] double BinMs() const noexcept { return BinMs_; }

  [[nodiscard]] Bound SlowestBound() const noexcept { return SlowestBound_; }

  [[nodiscard]] static constexpr bool IsGeometry(Held term) noexcept {
    return term == Held::Curvature || term == Held::Slip || term == Held::Ramp ||
           term == Held::Climb || term == Held::Crest;
  }

private:
  Bound Slowest_;
  Bound SlowestBound_;
  Bound Fastest_;
  std::array<size_t, static_cast<size_t>(Held::kCount)> Bound_ = {};

  static constexpr size_t kSpeedBins = 512;

  std::array<uint32_t, kSpeedBins> Bin_{};
  double BinMs_ = 0.0;
  std::vector<double> Held_;
  std::vector<Held> Why_;
  std::vector<double> Curvature_;
  double StepM_ = 0.0;
  double LengthM_ = 0.0;
};

} // namespace outshine

#endif
