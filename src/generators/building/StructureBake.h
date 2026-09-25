#ifndef OUTSHINE_GENERATORS_BUILDING_STRUCTUREBAKE_H
#define OUTSHINE_GENERATORS_BUILDING_STRUCTUREBAKE_H

#include <array>
#include <expected>
#include <atomic>
#include <variant>
#include <string_view>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "math/Vec3.h"
#include "BuildingField.h"
#include "HeightField.h"
#include "StructureMesher.h"
#include "spatial/Capacity.h"
#include "spatial/ClusterCook.h"
#include "scene/LevelOfDetail.h"
#include "StructureCell.h"

namespace outshine::Generators {

inline constexpr double kStructureEyeReuseM = 64.0;
inline constexpr double kStructureEyeDetailGuardM = 128.0;
static_assert(kStructureEyeDetailGuardM >= kStructureEyeReuseM);

enum class StructureBakeErrorKind {
  Cancelled,
  InvalidDetail,
  ChangedDetail,
  InvalidCell,
  ChangedCell
};

using StructureBakeError = std::variant<StructureMeshError, ClusterError, StructureBakeErrorKind>;

[[nodiscard]] inline std::string_view Describe(const StructureBakeError &error) noexcept {
  if (const auto *mesh = std::get_if<StructureMeshError>(&error)) {
    return outshine::Describe(*mesh);
  }
  if (const auto *cluster = std::get_if<ClusterError>(&error)) {
    return outshine::Describe(*cluster);
  }
  switch (std::get<StructureBakeErrorKind>(error)) {
    case StructureBakeErrorKind::Cancelled: return "structure bake cancelled";
    case StructureBakeErrorKind::InvalidDetail: return "unsupported structure detail level";
    case StructureBakeErrorKind::ChangedDetail: return "structure detail changed during bake";
    case StructureBakeErrorKind::InvalidCell: return "unsupported structure cell";
    case StructureBakeErrorKind::ChangedCell: return "structure cell changed during bake";
  }
  return "unknown structure bake error";
}

struct RawTile {
  struct Structure {
    uint32_t LocalFirst = 0;
    uint32_t PointCount = 0;
    uint32_t SourceFirst = 0;
    StructureCell Cell;
    double HeightM = 0.0;
    int Pitched = -1;
  };

  struct Way {
    uint32_t LocalFirst = 0;
    uint32_t PointCount = 0;
    float HalfWidthM = 0.0f;
  };

  std::vector<double> LatLon;
  std::vector<Structure> Structures;
  std::vector<Way> Ways;
  Vec3 AnchorEcef;
  LongitudeLatitude Eye;
  std::optional<LevelOfDetail> RequestedDetail;
  std::optional<uint32_t> RequestedCell;
  double FocalPx = 0.0;
  double TileSpanM = 0.0;
  int Extent = 4096;
  uint32_t ClusterTriangles = 0;

  [[nodiscard]] size_t HeapBytes() const noexcept {
    return CapacityBytes(LatLon) + CapacityBytes(Structures) + CapacityBytes(Ways);
  }
};

struct BakedTile {
  Raised Built;
  ClusteredMesh Walls, Roofs;
  uint64_t Digest = 0;
  bool FallbackHeights = false;
  std::optional<LevelOfDetail> RequestedDetail;
  std::optional<uint32_t> RequestedCell;
  std::optional<outshine::Ground::GeoBounds> FootprintBounds;
  uint64_t OccupiedCells = 0;
  std::array<outshine::Ground::GeoBounds, kStructureCellsPerTile> CellBounds{};
  std::array<float, kStructureCellsPerTile> CellMaxHeightM{};
  std::vector<outshine::Ground::BuildingField::Footprint> Prints;
  std::vector<LevelOfDetail> FootprintDetails;
  std::vector<double> SeatSpreadM;
  std::vector<double> AcrossM;
  int OsmHeights = 0;
  int DefaultHeights = 0;
  int Fronted = 0;
  int Lumped = 0;
  int Blocks = 0;
  int NoGround = 0;
  size_t UnsupportedMeshes = 0;

  [[nodiscard]] size_t HeapBytes() const noexcept {
    return Built.HeapBytes() + Walls.HeapBytes() + Roofs.HeapBytes() + CapacityBytes(Prints) +
           CapacityBytes(FootprintDetails) + CapacityBytes(SeatSpreadM) + CapacityBytes(AcrossM);
  }
};

class StructureBakeProgress {
public:
  StructureBakeProgress();
  ~StructureBakeProgress();
  StructureBakeProgress(const StructureBakeProgress &) = delete;
  StructureBakeProgress &operator=(const StructureBakeProgress &) = delete;

  [[nodiscard]] std::expected<bool, StructureBakeError>
  AdvanceStructures(const RawTile &raw,
                    const outshine::Ground::HeightField &heights,
                    const StructureMesher &mesher,
                    MeshScratch &scratch,
                    size_t structuresMost,
                    const std::atomic_bool *stopping = nullptr);

  [[nodiscard]] std::expected<BakedTile, StructureBakeError>
  Finalize(const RawTile &raw,
           const StructureMesher &mesher,
           MeshScratch &scratch,
           const std::atomic_bool *stopping = nullptr);

  [[nodiscard]] size_t BakedStructures() const noexcept;

private:
  struct State;
  std::unique_ptr<State> State_;
};

[[nodiscard]] std::expected<void, StructureBakeError>
BakeStructures(const RawTile &raw,
               const outshine::Ground::HeightField &heights,
               const StructureMesher &mesher,
               MeshScratch &scratch,
               BakedTile &out,
               const std::atomic_bool *stopping = nullptr);

}
#endif
