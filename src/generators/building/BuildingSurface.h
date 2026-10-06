#ifndef OUTSHINE_GENERATORS_BUILDING_BUILDINGSURFACE_H
#define OUTSHINE_GENERATORS_BUILDING_BUILDINGSURFACE_H

#include "BuildingShape.h"
#include "Geodesy.h"
#include "StoredVertex.h"
#include "math/Box.h"

#include <cstddef>
#include <array>
#include <expected>
#include <optional>
#include <vector>

namespace outshine::Generators {

struct BuildingSurfacePatch;

class BuildingSurface {
public:
  struct Ray {
    Vec3 Origin, Direction;
  };

  struct Hit {
    double Along = 0.0;
    Vec3 Normal;
    size_t Part = 0, Face = 0;
    bool Gable = false;
  };

  struct Face {
    size_t Part = 0, Side = 0;
    constexpr auto operator<=>(const Face &) const = default;
  };

  [[nodiscard]] static std::expected<BuildingSurface, StructureMeshError>
  Prepare(const StructurePlan &plan, BuildingScratch &scratch);

  [[nodiscard]] const Vec3 &Origin() const noexcept { return Origin_; }

  [[nodiscard]] const EnuAxes &Axes() const noexcept { return Axes_; }

  [[nodiscard]] std::span<const BuildingShape> Shapes() const noexcept { return Shapes_; }

  [[nodiscard]] const Box &Bounds() const noexcept { return Bounds_; }

  [[nodiscard]] size_t FaceCount() const noexcept { return FaceOffsets_.back(); }

  [[nodiscard]] size_t FaceIndex(const Hit &hit) const noexcept {
    return FaceOffsets_[hit.Part] + hit.Face;
  }

  [[nodiscard]] Face FaceAt(size_t index) const noexcept;
  [[nodiscard]] bool SupportsProjection() const noexcept;

  [[nodiscard]] std::optional<Hit>
  Trace(const Ray &ray, double minimum, double maximum, std::vector<double> &cuts) const;
  [[nodiscard]] StoredVertex VertexAt(const Hit &hit, const Vec3 &position) const noexcept;
  [[nodiscard]] Vec3f ColourFactor(const Hit &hit) const noexcept;

  [[nodiscard]] bool IsRoof(const Hit &hit) const noexcept {
    return hit.Face == 1 && Dot(hit.Normal, Axes_.Up) > kSteepestRoof;
  }

  [[nodiscard]] std::expected<void, StructureMeshError>
  MeshVisible(std::span<const Face> faces, BuildingScratch &scratch, Raised &into) const;

  [[nodiscard]] size_t EstimatedTriangles(std::span<const Face> faces) const noexcept;
  void MeshPatches(std::span<const BuildingSurfacePatch> patches, Raised &into) const;

private:
  friend class PreparedStructureCodec;
  Vec3 Origin_;
  EnuAxes Axes_;
  Box Bounds_;
  double MinimumHeightM_ = 0.0;
  std::optional<Vec3f> WallColour_;
  std::vector<BuildingShape> Shapes_;
  std::vector<size_t> FaceOffsets_;
};

struct BuildingSurfacePatch {
  BuildingSurface::Hit Sample;
  std::array<Vec3, 4> Corners;
};

}
#endif
