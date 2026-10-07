#ifndef OUTSHINE_GENERATORS_BUILDING_STRUCTUREMESHER_H
#define OUTSHINE_GENERATORS_BUILDING_STRUCTUREMESHER_H

#include "math/Box.h"

#include <cstddef>
#include <expected>
#include <string_view>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "math/Vec3.h"

#include <scene/LevelOfDetail.h>
#include <scene/ProjectedErrorBudget.h>

#include "StoredVertex.h"
#include "BuildingFrontage.h"
#include "GeographicRing.h"
#include "TileMeshes.h"

namespace outshine {

namespace Generators {
class BuildingSurface;
}

inline constexpr double kSteepestRoof = 0.5;

struct Raised {
  std::vector<StoredVertex> WallCorners, RoofCorners;
  std::vector<uint32_t> WallRun, RoofRun;
  std::vector<float> WallColours;

  void Clear() noexcept {
    WallCorners.clear();
    RoofCorners.clear();
    WallRun.clear();
    RoofRun.clear();
    WallColours.clear();
  }

  void Settle() {
    WallCorners.shrink_to_fit();
    RoofCorners.shrink_to_fit();
    WallRun.shrink_to_fit();
    RoofRun.shrink_to_fit();
    WallColours.shrink_to_fit();
  }

  [[nodiscard]] std::size_t HeapBytes() const noexcept {
    return WallCorners.capacity() * sizeof(StoredVertex) +
           RoofCorners.capacity() * sizeof(StoredVertex) + WallRun.capacity() * sizeof(uint32_t) +
           RoofRun.capacity() * sizeof(uint32_t) + WallColours.capacity() * sizeof(float);
  }

  [[nodiscard]] std::size_t UsedBytes() const noexcept {
    return WallCorners.size() * sizeof(StoredVertex) + RoofCorners.size() * sizeof(StoredVertex) +
           WallRun.size() * sizeof(uint32_t) + RoofRun.size() * sizeof(uint32_t) +
           WallColours.size() * sizeof(float);
  }
};

struct WayLine {
  std::span<const double> LatLon;
  double HalfWidthM = 0.0;
  double MinLat = 0.0, MinLon = 0.0, MaxLat = 0.0, MaxLon = 0.0;
};

inline constexpr double kPitchedShareUnknown = -1.0;

struct StructurePlan {
  const Generators::BuildingSurface *Prepared = nullptr;
  std::span<const double> RingLatLon;
  std::span<const GeographicRing> InnerRings;
  std::span<const double> RingPointsLatLon;
  double BaseAslM = 0.0;

  double SeatAslM = 0.0;
  double FootAslM = 0.0;

  std::span<const double> CornerAslM;

  double HeightM = 0.0;
  double MinimumHeightM = 0.0;
  bool HeightMeasured = false;
  BuildingFrontage Street;

  Vec3 AnchorEcef;

  LevelOfDetail Coarseness = LevelOfDetail::Fine;
  bool RecessedOpenings = true;

  double PitchedShare = kPitchedShareUnknown;
  std::optional<Vec3f> WallColour;
};

class MeshScratch {
public:
  virtual ~MeshScratch() = default;
  MeshScratch(const MeshScratch &) = delete;
  MeshScratch &operator=(const MeshScratch &) = delete;

protected:
  MeshScratch() = default;
};

enum class StructureMeshError {
  InvalidPlan,
  IncompatibleScratch,
  AllocationFailed,
  BuildFailed,
  UnsupportedFootprint,
  PreparedSurfaceUnavailable
};

struct ProjectedStructureMesh {
  Raised Mesh;
  double EyeRadiusM = 0.0;
};

[[nodiscard]] constexpr std::string_view Describe(StructureMeshError error) noexcept {
  switch (error) {
    case StructureMeshError::InvalidPlan: return "invalid structure plan";
    case StructureMeshError::IncompatibleScratch: return "incompatible structure mesh scratch";
    case StructureMeshError::AllocationFailed: return "structure mesh storage allocation failed";
    case StructureMeshError::BuildFailed: return "structure mesh construction failed";
    case StructureMeshError::UnsupportedFootprint:
      return "footprint cannot form a supported building mass";
    case StructureMeshError::PreparedSurfaceUnavailable:
      return "prepared building surface could not be loaded";
  }
  return "unknown structure mesh error";
}

class StructureMesher {
public:
  virtual ~StructureMesher() = default;
  StructureMesher(const StructureMesher &) = delete;
  StructureMesher &operator=(const StructureMesher &) = delete;

  [[nodiscard]] virtual std::string_view ArtifactVersion() const noexcept { return {}; }

  [[nodiscard]] virtual std::optional<double>
  ShellSurfaceErrorM([[maybe_unused]] std::span<const StoredVertex> walls) const noexcept {
    return std::nullopt;
  }

  [[nodiscard]] virtual std::optional<double>
  ShellSurfaceErrorM([[maybe_unused]] const StructurePlan &plan,
                     [[maybe_unused]] MeshScratch &scratch) const noexcept {
    return std::nullopt;
  }

  [[nodiscard]] virtual std::unique_ptr<MeshScratch> Scratch() const = 0;

  [[nodiscard]] virtual bool HasSurfaceProjection() const noexcept { return false; }

  [[nodiscard]] virtual std::expected<ProjectedStructureMesh, StructureMeshError>
  Project([[maybe_unused]] std::span<const StructurePlan> plans,
          [[maybe_unused]] const Vec3 &eye,
          [[maybe_unused]] ProjectedErrorBudget projection,
          [[maybe_unused]] MeshScratch &scratch) const {
    return std::unexpected(StructureMeshError::UnsupportedFootprint);
  }

  [[nodiscard]] virtual std::optional<Box>
  SourceEnvelopeBounds([[maybe_unused]] const StructurePlan &plan,
                       [[maybe_unused]] MeshScratch &scratch) const noexcept {
    return std::nullopt;
  }

  [[nodiscard]] virtual std::expected<void, StructureMeshError>
  Mesh(const StructurePlan &plan, MeshScratch &scratch, Raised &into) const noexcept = 0;

protected:
  StructureMesher() = default;
};

}
#endif
