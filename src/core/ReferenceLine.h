#ifndef OUTSHINE_REFERENCELINE_H
#define OUTSHINE_REFERENCELINE_H

#include <cstddef>
#include <string>
#include <vector>

namespace outshine {

inline constexpr size_t kMaxCorridorSegments = 4096;
inline constexpr double kTangentTolerance = 1.0e-9;

enum class Curve : uint8_t { Straight, Arc, Spiral };

struct Placed {
  double EastM = 0.0;
  double NorthM = 0.0;
  double HeadingRad = 0.0;
  double CurvaturePerM = 0.0;
};

struct Segment {
  Curve Shape = Curve::Straight;
  double LengthM = 0.0;
  double EntryCurvature = 0.0;
  double ExitCurvature = 0.0;
};

class ReferenceLine {
public:
  [[nodiscard]] bool Lay(const Placed &from, const std::vector<Segment> &along, std::string &error);

  [[nodiscard]] bool At(double alongM, Placed &out) const;
  [[nodiscard]] double LengthM(void) const { return Length_; }
  [[nodiscard]] size_t SegmentCount(void) const { return Laid_.size(); }
  [[nodiscard]] const std::string &Error(void) const { return Error_; }

private:
  struct Held {
    Segment Declared;
    Placed Entry;
    double AlongM = 0.0;
  };
  [[nodiscard]] static Placed Walk(const Placed &from, const Segment &along, double byM);
  [[nodiscard]] bool Refuse(const std::string &why);

  std::vector<Held> Laid_;
  double Length_ = 0.0;
  Placed End_;
  std::string Error_;
};

} // namespace outshine

#endif
