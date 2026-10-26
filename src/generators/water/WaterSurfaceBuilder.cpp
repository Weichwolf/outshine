#include "WaterSurfaceBuilder.h"
#include "WaterSurfaceMesh.h"
#include "TangentFrame.h"
#include "Earth.h"
#include "math/RenderFrame.h"
#include "Vec3.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace outshine::Generators {
namespace {
constexpr float kWaterRoughness = 0.08f;
constexpr float kWaterIor = 1.333f;

Vec3 PositionIn(const Vec3 &point, const TangentFrame &source, const TangentFrame &target) {
  if (source.Anchor() == target.Anchor()) { return point; }
  const Vec3 ecef = source.ToEcefPosition({.EastM = point[0], .NorthM = point[1], .UpM = point[2]});
  const auto placed = target.ToLocalPosition(ecef);
  return {{placed.EastM, placed.NorthM, placed.UpM}};
}

Vec3 NormalIn(const Vec3 &normal, const TangentFrame &source, const TangentFrame &target) {
  const double magnitude = std::hypot(normal[0], normal[1], normal[2]);
  const Vec3 unit = magnitude > 0 ? normal * (1.0 / magnitude) : Vec3{{0, 0, 1}};
  if (source.Anchor() == target.Anchor()) { return unit; }
  const Vec3 ecef = source.ToEcefDirection({.EastM = unit[0], .NorthM = unit[1], .UpM = unit[2]});
  const auto placed = target.ToLocalDirection(ecef);
  return {{placed.EastM, placed.NorthM, placed.UpM}};
}
}

std::optional<double> WaterSurfaceUpAt(const WaterAsset &water,
                                       const TangentFrame &frame,
                                       LongitudeLatitude at) noexcept {
  const auto level = water.LevelAt(at);
  if (!level) { return std::nullopt; }
  return frame
      .ToLocalPosition(
          {.LongitudeDeg = at.LongitudeDeg, .LatitudeDeg = at.LatitudeDeg, .HeightM = *level})
      .UpM;
}

Material WaterSurfaceMaterial() noexcept {
  Material material;
  material.BaseColour = {{1.0f, 1.0f, 1.0f, 1.0f}};
  material.Roughness = kWaterRoughness;
  material.Transmission = 1.0f;
  material.Ior = kWaterIor;
  material.DoubleSided = true;
  return material;
}

WaterSurfaceMetrics InspectWaterSurfaceGeometry(const WaterAsset &water) noexcept {
  WaterSurfaceMetrics metrics;
  const auto *prepared = water.PreparedSurfaces();
  if (prepared == nullptr) { return metrics; }
  metrics.ProfileDeclines = prepared->ProfileDeclines.size();
  for (const auto &rejection : prepared->Rejections) {
    if (rejection.Reason == WaterAsset::SurfaceRejectionReason::CapacityExceeded) {
      ++metrics.RefusedCapacity;
    } else {
      ++metrics.RefusedTopology;
    }
  }
  metrics.Laid = water.Surfaces().size() - prepared->Rejections.size();
  metrics.Triangles = prepared->Mesh.Faces().size();
  return metrics;
}

std::expected<WaterSurfaceMetrics, std::string>
AppendWaterSurfaceGeometry(Geometry &geometry,
                           MaterialInstance material,
                           const WaterAsset &water,
                           const TangentFrame &frame) {
  if (!water.SurfacesReady()) { return std::unexpected("water surfaces have not been prepared"); }
  const auto metrics = InspectWaterSurfaceGeometry(water);
  const auto *prepared = water.PreparedSurfaces();
  if (prepared == nullptr) { return metrics; }
  const auto &mesh = prepared->Mesh;
  if (mesh.Faces().empty()) { return metrics; }
  const auto source = TangentFrame::At(mesh.Anchor());
  const auto vertices = mesh.VerticesEnuM();
  std::vector<Vec3> smoothNormals(vertices.size());
  std::vector<uint32_t> indices;
  indices.reserve(mesh.Faces().size() * 3);
  for (const auto &face : mesh.Faces()) {
    const Vec3 normal = Cross(vertices[face.Vertices[1]] - vertices[face.Vertices[0]],
                              vertices[face.Vertices[2]] - vertices[face.Vertices[0]]);
    for (const uint32_t at : face.Vertices) {
      smoothNormals[at] = smoothNormals[at] + normal;
      indices.push_back(at);
    }
  }
  std::vector<float> positions;
  std::vector<float> normals;
  std::vector<float> texcoords;
  positions.reserve(vertices.size() * 3);
  normals.reserve(vertices.size() * 3);
  texcoords.reserve(vertices.size() * 2);
  for (size_t at = 0; at < vertices.size(); ++at) {
    const Vec3 placed = PositionIn(vertices[at], source, frame);
    const Vec3 normal = NormalIn(smoothNormals[at], source, frame);
    positions.insert(positions.end(),
                     {static_cast<float>(placed[0]),
                      static_cast<float>(placed[2]),
                      static_cast<float>(RenderFrame::ZOfNorth(placed[1]))});
    normals.insert(normals.end(),
                   {static_cast<float>(normal[0]),
                    static_cast<float>(normal[2]),
                    static_cast<float>(RenderFrame::ZOfNorth(normal[1]))});
    texcoords.insert(texcoords.end(),
                     {static_cast<float>(placed[0]), static_cast<float>(placed[1])});
  }
  const auto part = geometry.addPart("water", material);
  if (!part || !geometry.setPositions(*part, positions) || !geometry.setNormals(*part, normals) ||
      !geometry.setTriangles(*part, indices) || !geometry.setTexture(*part, texcoords, 0)) {
    return std::unexpected("could not append native water surface geometry");
  }
  return metrics;
}
}
