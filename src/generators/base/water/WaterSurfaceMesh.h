#ifndef OUTSHINE_GENERATORS_BASE_WATER_WATERSURFACEMESH_H
#define OUTSHINE_GENERATORS_BASE_WATER_WATERSURFACEMESH_H

#include "Earth.h"
#include "PlanHierarchy.h"
#include "TangentFrame.h"
#include "Vec3.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <vector>

namespace outshine::Generators {

enum class WaterMeshError { InvalidGeometry, InvalidStorage, CapacityExceeded };

class WaterSurfaceMesh {
public:
  struct Face {
    std::array<uint32_t, 3> Vertices{};
    uint32_t Surface = 0;
  };

  [[nodiscard]] static std::expected<WaterSurfaceMesh, WaterMeshError>
  Build(LongitudeLatitude anchor, std::vector<Vec3> verticesEnuM, std::vector<Face> faces);

  [[nodiscard]] static std::expected<WaterSurfaceMesh, WaterMeshError>
  Restore(LongitudeLatitude anchor,
          std::vector<Vec3> verticesEnuM,
          std::vector<Face> faces,
          std::vector<PlanHierarchy::Node> nodes,
          std::vector<uint32_t> order);

  [[nodiscard]] LongitudeLatitude Anchor() const noexcept { return Frame_.Anchor(); }

  [[nodiscard]] std::span<const Vec3> VerticesEnuM() const noexcept { return Vertices_; }

  [[nodiscard]] std::span<const Face> Faces() const noexcept { return Faces_; }

  [[nodiscard]] const PlanHierarchy &Hierarchy() const noexcept { return Hierarchy_; }

  [[nodiscard]] std::optional<double> LevelAt(LongitudeLatitude at) const noexcept;
  [[nodiscard]] size_t HeapBytes() const noexcept;

private:
  TangentFrame Frame_;
  std::vector<Vec3> Vertices_;
  std::vector<Face> Faces_;
  PlanHierarchy Hierarchy_;
};
}
#endif
