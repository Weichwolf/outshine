#ifndef OUTSHINE_CORRIDOR_RIBBON_H
#define OUTSHINE_CORRIDOR_RIBBON_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "ReferenceLine.h"

namespace outshine {

inline constexpr size_t kRibbonAcross = 4;

struct Section {
  double HalfWidthM = 0.0;
  double ShoulderM = 0.0;
  double ThicknessM = 0.0;
};

struct Ribbon {
  bool Woven = false;
  size_t Stations = 0;
  size_t Vertices = 0;
  size_t Triangles = 0;
  double FromM = 0.0;
  double ToM = 0.0;
  double TopAreaM2 = 0.0;

  double OriginM[3] = {0.0, 0.0, 0.0};
  std::vector<float> PositionM;
  std::vector<float> NormalM;
  std::vector<float> AcrossM;
  std::vector<uint32_t> Index;
  std::string Error;
};

[[nodiscard]] Ribbon Sweep(const ReferenceLine &along, const Section &section, double fromM,
                           double toM, double stepM);

} // namespace outshine

#endif
