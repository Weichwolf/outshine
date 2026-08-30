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
  int Levels = 1;
  int Grid = 33;
  bool Asking = false;
  double EyeM[3] = {0.0, 0.0, 0.0};
  float FocalPx = 0.0f;
  float Tau = kPixelTau;
  float Up[3] = {0.0f, 1.0f, 0.0f};
};

struct Patchwork {
  std::vector<float> PositionM;
  std::vector<float> NormalM;
  std::vector<float> Uv;
  std::vector<uint32_t> Index;

  std::vector<DagCluster> Clusters;
  std::vector<uint32_t> AllIndex;
  double ClusterEyeM[3] = {0.0, 0.0, 0.0};
  double OriginEcef[3] = {0.0, 0.0, 0.0};
  size_t Tiles = 0;
  size_t Pending = 0;
  size_t Absent = 0;
  size_t Refused = 0;
  double WorstErrM = 0.0;
  size_t ClustersHeld = 0;
  size_t ClustersDrawn = 0;
  size_t Skipped = 0;
  size_t Bare = 0;
  int PendingAtZoom[24] = {};
  int WantedAtZoom[24] = {};
  size_t Overlapped = 0;
  long ReachTiles = 0;
  int CoarsestZoom = 0;
};

void NormalsFrom(const std::vector<float> &positionM,
                 const std::vector<uint32_t> &index,
                 std::vector<float> &into);

[[nodiscard]] std::expected<Patchwork, std::string> LayPatchwork(TileMeshes &tiles,
                                                                 const Around &over);

}

#endif
