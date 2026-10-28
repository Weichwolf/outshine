#ifndef OUTSHINE_GENERATORS_BUILDING_BUILDINGMESHCONVERSION_H
#define OUTSHINE_GENERATORS_BUILDING_BUILDINGMESHCONVERSION_H

#include "Geodesy.h"
#include "Geometry.h"
#include "StoredVertex.h"
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>

namespace outshine::Generators {
struct BuildingMeshPart {
  std::span<const StoredVertex> Vertices;
  std::span<const uint32_t> Indices;
  std::span<const float> Colours;
  std::span<const float> MaterialM;
};

[[nodiscard]] std::expected<Geometry::Mesh, std::string_view>
ConvertBuildingMesh(const BuildingMeshPart &part, const EnuAxes &frame);
}
#endif
