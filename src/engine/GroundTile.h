#ifndef OUTSHINE_ENGINE_GROUNDTILE_H
#define OUTSHINE_ENGINE_GROUNDTILE_H

#include "ResourceHandle.h"
#include "math/Mat4.h"
#include <array>

namespace outshine::Core {
struct GroundTile {
  Mat4f Row = {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
  std::array<float, 8> Corners{};
  HeightPageHandle Page{};
  float SagInv = 0;
  float StepE = 0;
  float StepN = 0;
  float LowM = 0;
  float HighM = 0;
};
}
#endif
