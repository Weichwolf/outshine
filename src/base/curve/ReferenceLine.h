#ifndef OUTSHINE_BASE_CURVE_REFERENCELINE_H
#define OUTSHINE_BASE_CURVE_REFERENCELINE_H

#include "Earth.h"
#include <optional>
#include "math/Units.h"
#include <cstddef>
#include <initializer_list>
#include <span>
#include <string>
#include <vector>

namespace outshine {

struct Nearby {
  double AboutM = 0.0;
  double WithinM = 0.0;
};

struct Curving {
  double Value = 0.0;
  double Rate = 0.0;
  double Bend = 0.0;
};

inline constexpr size_t kMaxCorridorSegments = 262144;
inline constexpr size_t kMaxCorridorKnots = 262144;
inline constexpr double kResectionCoarseM = 1.0;
inline constexpr int kResectionRefinements = 24;
inline constexpr double kTangentTolerance = kLeastTurnRad;

enum class Curve : uint8_t { Straight, Arc, Spiral };

struct Placed {
  double EastM = 0.0;
  double NorthM = 0.0;
  double HeightM = 0.0;
  double HeadingRad = 0.0;
  double CurvaturePerM = 0.0;
  double CurvatureRatePerM = 0.0;
  double Slope = 0.0;
  double SlopeRatePerM = 0.0;
  double BankRad = 0.0;
  double BankRatePerM = 0.0;
};

struct Knot {
  double AlongM = 0.0;
  double Value = 0.0;
  double RatePerM = 0.0;
};

struct Segment {
  Curve Shape = Curve::Straight;
  double LengthM = 0.0;
  double EntryCurvature = 0.0;
  double ExitCurvature = 0.0;
};

class ReferenceLine {
public:
  [[nodiscard]] bool Lay(const Placed &from, std::span<const Segment> along, std::string &error);

  [[nodiscard]] bool
  Lay(const Placed &from, std::initializer_list<Segment> along, std::string &error) {
    return Lay(from, std::span<const Segment>(along.begin(), along.size()), error);
  }

  [[nodiscard]] bool Rise(std::span<const Knot> through, std::string &error);

  [[nodiscard]] bool Rise(std::initializer_list<Knot> through, std::string &error) {
    return Rise(std::span<const Knot>(through.begin(), through.size()), error);
  }

  [[nodiscard]] bool Bank(std::span<const Knot> through, std::string &error);

  [[nodiscard]] bool Bank(std::initializer_list<Knot> through, std::string &error) {
    return Bank(std::span<const Knot>(through.begin(), through.size()), error);
  }

  [[nodiscard]] bool At(double alongM, Placed &out) const;
  [[nodiscard]] std::optional<double> Nearest(EastNorth at, Nearby about) const;

  [[nodiscard]] double LengthM() const { return Length_; }

  [[nodiscard]] size_t SegmentCount() const { return Laid_.size(); }

  [[nodiscard]] size_t RiseKnotCount() const { return Rise_.size(); }

  [[nodiscard]] size_t BankKnotCount() const { return Bank_.size(); }

  [[nodiscard]] std::vector<double> Seams() const;

  [[nodiscard]] const std::string &Error() const { return Error_; }

private:
  struct Held {
    Segment Declared;
    Placed Entry;
    double AlongM = 0.0;
  };

  [[nodiscard]] static Placed Walk(const Placed &from, const Segment &along, double byM);
  [[nodiscard]] bool Refuse(std::string why);
  [[nodiscard]] bool Fasten(std::span<const Knot> through,
                            const char *what,
                            const char *unit,
                            std::vector<Knot> &into,
                            std::string &error);
  [[nodiscard]] static Curving Read(std::span<const Knot> through, double alongM);

  std::vector<Held> Laid_;
  std::vector<Knot> Rise_;
  std::vector<Knot> Bank_;
  double Length_ = 0.0;
  Placed End_;
  std::string Error_;
};

} // namespace outshine

#endif
