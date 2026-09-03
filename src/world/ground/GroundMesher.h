#ifndef OUTSHINE_WORLD_GROUND_GROUNDMESHER_H
#define OUTSHINE_WORLD_GROUND_GROUNDMESHER_H

#include <array>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

#include "math/Vec3.h"

#include "TileMeshes.h"

namespace outshine {

constexpr size_t kZoomLevels = 24;

constexpr int kPatchGrid = 33;

struct Around {
  double LatitudeDeg = 0.0;
  double LongitudeDeg = 0.0;
  int Zoom = 0;
  int Levels = 1;
  int Grid = kPatchGrid;
  bool Asking = false;
  Vec3 EyeM = {{0.0, 0.0, 0.0}};
  float FocalPx = 0.0f;
  float Tau = kPixelTau;
  Vec3f Up = {{0.0f, 1.0f, 0.0f}};
};

struct Patchwork {
  std::vector<float> PositionM;
  std::vector<float> NormalM;
  std::vector<float> Uv;
  std::vector<uint32_t> Index;

  std::vector<DagCluster> Clusters;
  std::vector<uint32_t> AllIndex;
  Vec3 ClusterEyeM = {{0.0, 0.0, 0.0}};
  Vec3 OriginEcef = {{0.0, 0.0, 0.0}};
  size_t Tiles = 0;
  size_t Pending = 0;
  size_t Absent = 0;
  size_t Refused = 0;
  double WorstErrM = 0.0;
  size_t ClustersHeld = 0;
  size_t ClustersDrawn = 0;
  size_t Skipped = 0;
  size_t Bare = 0;
  std::array<int, kZoomLevels> PendingAtZoom = {{}};
  std::array<int, kZoomLevels> WantedAtZoom = {{}};
  size_t Overlapped = 0;
  long ReachTiles = 0;
  int CoarsestZoom = 0;
};

void NormalsFrom(const std::vector<float> &positionM,
                 const std::vector<uint32_t> &index,
                 std::vector<float> &into);

struct EastSouth {
  double EastM = 0.0;
  double SouthM = 0.0;
};

struct Yields {
  std::vector<double> RingEastSouthM;
  double LowE = 0.0, HighE = 0.0, LowS = 0.0, HighS = 0.0;
  double AtE = 0.0, AtS = 0.0;
  double PlateauM = 0.0;
  double SlopeE = 0.0, SlopeS = 0.0;
  std::vector<double> SeamEastSouthM;
  double ApronM = 0.0;
  double YieldM = 0.0;
  bool Fills = false;

  [[nodiscard]] double WantsAt(EastSouth at) const {
    return PlateauM + SlopeE * (at.EastM - AtE) + SlopeS * (at.SouthM - AtS);
  }
};

struct GroundMesh {
  std::vector<float> *PositionM = nullptr;
  std::vector<float> *NormalM = nullptr;
  std::vector<float> *ColourRgba = nullptr;
  std::vector<float> *Uv = nullptr;
  std::vector<uint32_t> *Index = nullptr;

  int (*ClassAt)(const void *with, EastSouth at) = nullptr;
  const void *With = nullptr;
};

struct Yielded {
  size_t Divided = 0;
  double RefineMs = 0.0;
  double CutMs = 0.0;
  double SewMs = 0.0;
  double PressMs = 0.0;
  double SeamMs = 0.0;
  size_t Seams = 0;
  size_t SeamsShared = 0;
  size_t Taken = 0;
  size_t Refused = 0;
  size_t VerticesAdded = 0;
  size_t TrianglesAdded = 0;
  size_t Pressed = 0;
  double DeepestM = 0.0;
  double RaisedM = 0.0;
  size_t Passes = 0;
};

struct Budget {
  double FinestM = 0.0;
  size_t MostTriangles = 0;
};

class GroundMesher {
public:
  virtual ~GroundMesher() = default;
  GroundMesher(const GroundMesher &) = delete;
  GroundMesher &operator=(const GroundMesher &) = delete;

  [[nodiscard]] virtual std::expected<Patchwork, std::string> Lay(TileMeshes &tiles,
                                                                  const Around &over) const = 0;

  virtual void
  Yield(std::span<const Yields> these, Budget within, GroundMesh mesh, Yielded &told) const = 0;

protected:
  GroundMesher() = default;
};

} // namespace outshine
#endif
