#ifndef OUTSHINE_COMPOSITOR_GROUNDPATCHWORK_H
#define OUTSHINE_COMPOSITOR_GROUNDPATCHWORK_H

#include <cstdint>
#include <expected>
#include <string>
#include <vector>

#include "TileMeshes.h"

namespace outshine {

struct Around {
  double LatDeg = 0.0;
  double LonDeg = 0.0;
  int Zoom = 0;
  int Ring = 1;
  int Grid = 33;
  bool Awaited = false;
};

struct Patchwork {
  std::vector<float> PositionM;
  std::vector<float> NormalM;
  std::vector<float> Uv;
  std::vector<uint32_t> Index;
  double OriginEcef[3] = {0.0, 0.0, 0.0};
  size_t Tiles = 0;
  size_t Pending = 0;
  size_t Absent = 0;
  size_t Refused = 0;
  double WorstErrM = 0.0;
};

void NormalsFrom(const std::vector<float> &positionM, const std::vector<uint32_t> &index,
                 std::vector<float> &into);

[[nodiscard]] std::expected<Patchwork, std::string> LayPatchwork(TileMeshes &tiles,
                                                                 const Around &over);

}

#endif
