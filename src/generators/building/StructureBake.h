#ifndef OUTSHINE_GENERATORS_BUILDING_STRUCTUREBAKE_H
#define OUTSHINE_GENERATORS_BUILDING_STRUCTUREBAKE_H

#include <expected>
#include <atomic>
#include <variant>
#include <string_view>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "math/Vec3.h"
#include "BuildingField.h"
#include "HeightField.h"
#include "StructureMesher.h"
#include "spatial/ClusterCook.h"

namespace outshine::Generators {

enum class StructureBakeErrorKind { Cancelled };

using StructureBakeError = std::variant<StructureMeshError, ClusterError, StructureBakeErrorKind>;

[[nodiscard]] inline std::string_view Describe(const StructureBakeError &error) noexcept {
  if (const auto *mesh = std::get_if<StructureMeshError>(&error)) {
    return outshine::Describe(*mesh);
  }
  if (const auto *cluster = std::get_if<ClusterError>(&error)) {
    return outshine::Describe(*cluster);
  }
  return "structure bake cancelled";
}

struct RawTile {
  struct Structure {
    uint32_t LocalFirst = 0;
    uint32_t PointCount = 0;
    uint32_t SourceFirst = 0;
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
  double FocalPx = 0.0;
  double TileSpanM = 0.0;
  int Extent = 4096;
  uint32_t ClusterTriangles = 0;
};

struct BakedTile {
  Raised Built;
  ClusteredMesh Walls, Roofs;
  uint64_t Digest = 0;
  std::vector<outshine::Ground::BuildingField::Footprint> Prints;
  std::vector<double> SeatSpreadM;
  std::vector<double> AcrossM;
  int OsmHeights = 0;
  int DefaultHeights = 0;
  int Fronted = 0;
  int Lumped = 0;
  int Blocks = 0;
  int NoGround = 0;
  size_t UnsupportedMeshes = 0;
};

class StructureBakeProgress {
public:
  StructureBakeProgress();
  ~StructureBakeProgress();
  StructureBakeProgress(const StructureBakeProgress &) = delete;
  StructureBakeProgress &operator=(const StructureBakeProgress &) = delete;

  [[nodiscard]] std::expected<bool, StructureBakeError>
  Advance(const RawTile &raw,
          const outshine::Ground::HeightField &heights,
          const StructureMesher &mesher,
          MeshScratch &scratch,
          BakedTile &out,
          size_t structuresMost,
          const std::atomic_bool *stopping = nullptr);

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
