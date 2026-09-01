#ifndef OUTSHINE_COMPOSITOR_GROUNDYIELD_H
#define OUTSHINE_COMPOSITOR_GROUNDYIELD_H

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace outshine {

struct Yields {
  std::vector<double> RingEastSouthM;
  double LowE = 0.0, HighE = 0.0, LowS = 0.0, HighS = 0.0;
  double AtE = 0.0, AtS = 0.0;
  double PlateauM = 0.0;
  double SlopeE = 0.0, SlopeS = 0.0;
  double ApronM = 0.0;
  double YieldM = 0.0;
  bool Fills = false;

  [[nodiscard]] double WantsAt(double eastM, double southM) const {
    return PlateauM + SlopeE * (eastM - AtE) + SlopeS * (southM - AtS);
  }
};

struct GroundMesh {
  std::vector<float> *PositionM = nullptr;
  std::vector<float> *NormalM = nullptr;
  std::vector<float> *ColourRgba = nullptr;
  std::vector<float> *Uv = nullptr;
  std::vector<uint32_t> *Index = nullptr;
};

struct Yielded {
  size_t Taken = 0;
  size_t Refused = 0;
  size_t VerticesAdded = 0;
  size_t TrianglesAdded = 0;
  size_t Pressed = 0;
  double DeepestM = 0.0;
  double RaisedM = 0.0;
  size_t Passes = 0;
};

void YieldGround(std::span<const Yields> these,
                 double finestM,
                 size_t mostTriangles,
                 GroundMesh mesh,
                 Yielded &told);

} // namespace outshine
#endif
