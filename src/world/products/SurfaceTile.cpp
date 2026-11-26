#include "SurfaceTile.h"
#include "ReadTextFile.h"
#include "Units.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <nlohmann/json.hpp>

namespace outshine {
namespace {
using Json = nlohmann::json;
constexpr size_t kFileBytesMost = size_t{8} * 1024 * 1024;
constexpr size_t kRevisionLength = 64;
constexpr double kCoordinateLimitM = 2e7;

bool UnsignedAtMost(const Json &value, uint32_t limit) {
  return value.is_number_unsigned() && value.get<uint64_t>() <= limit;
}

bool Numbers(const Json &value, size_t count) {
  return value.is_array() && value.size() == count &&
         std::ranges::all_of(value, [](const Json &entry) {
           return entry.is_number() && std::abs(entry.get<double>()) <= kCoordinateLimitM;
         });
}

Vec3 Triple(const Json &row) {
  return {{row[0].get<double>(), row[1].get<double>(), row[2].get<double>()}};
}

bool ReadHeader(const Json &record, SurfaceTile &tile) {
  if (!record.contains("version") || record["version"] != SurfaceTile::Version ||
      !record.contains("coverage") || !record.contains("origin_lon_lat") ||
      !record.contains("revision") || !record.contains("bounds_eus_m") ||
      !record.contains("geometric_error_m")) {
    return false;
  }
  const auto &coverage = record["coverage"];
  const auto &origin = record["origin_lon_lat"];
  const auto &revision = record["revision"];
  const auto &bounds = record["bounds_eus_m"];
  const auto &error = record["geometric_error_m"];
  if (!coverage.is_array() || coverage.size() != 3 ||
      !UnsignedAtMost(coverage[0], Data::TileId::MaximumZoom) || !Numbers(origin, 2) ||
      !revision.is_string() || !bounds.is_array() || bounds.size() != 2 || !Numbers(bounds[0], 3) ||
      !Numbers(bounds[1], 3) || !error.is_number()) {
    return false;
  }
  const auto zoom = coverage[0].get<unsigned>();
  const auto last = (1u << zoom) - 1;
  if (!UnsignedAtMost(coverage[1], last) || !UnsignedAtMost(coverage[2], last)) { return false; }
  tile.Coverage = {.Zoom = static_cast<int>(zoom),
                   .X = coverage[1].get<uint32_t>(),
                   .Y = coverage[2].get<uint32_t>()};
  tile.Origin = {.LongitudeDeg = origin[0].get<double>(), .LatitudeDeg = origin[1].get<double>()};
  tile.Revision = revision.get<std::string>();
  tile.MinimumM = Triple(bounds[0]);
  tile.MaximumM = Triple(bounds[1]);
  tile.GeometricErrorM = error.get<double>();
  return std::abs(tile.Origin.LongitudeDeg) <= kDegPerHalfTurn &&
         std::abs(tile.Origin.LatitudeDeg) <= kDegPerHalfTurn / 2 && tile.GeometricErrorM >= 0 &&
         tile.GeometricErrorM <= kCoordinateLimitM && tile.Revision.size() == kRevisionLength &&
         std::ranges::all_of(tile.Revision,
                             [](unsigned char value) { return std::isxdigit(value) != 0; });
}

bool ReadMesh(const Json &record, SurfaceTile &tile) {
  if (!record.contains("vertices_eus_m") || !record.contains("normals_eus") ||
      !record.contains("indices")) {
    return false;
  }
  const auto &vertices = record["vertices_eus_m"];
  const auto &normals = record["normals_eus"];
  const auto &indices = record["indices"];
  if (!vertices.is_array() || vertices.size() < 3 ||
      vertices.size() > SurfaceTile::MaximumVertices || !indices.is_array() || indices.empty() ||
      indices.size() % 3 != 0 || indices.size() > SurfaceTile::MaximumIndices) {
    return false;
  }
  tile.Vertices.reserve(vertices.size());
  tile.Indices.reserve(indices.size());
  for (size_t at = 0; at < vertices.size(); ++at) {
    if (!Numbers(vertices[at], 3) || !Numbers(normals[at], 3)) { return false; }
    const auto position = Triple(vertices[at]);
    const auto normal = Triple(normals[at]);
    constexpr double kNormalSquaredTolerance = 0.001;
    if (std::abs(Dot(normal, normal) - 1.0) > kNormalSquaredTolerance) { return false; }
    SurfaceVertex packed;
    for (size_t axis = 0; axis < 3; ++axis) {
      packed.PositionM[axis] = static_cast<float>(position[axis]);
      packed.Normal[axis] = static_cast<float>(normal[axis]);
      if (packed.PositionM[axis] < tile.MinimumM[axis] ||
          packed.PositionM[axis] > tile.MaximumM[axis]) {
        return false;
      }
    }
    tile.Vertices.push_back(packed);
  }
  const auto last = static_cast<uint32_t>(vertices.size() - 1);
  for (const auto &index : indices) {
    if (!UnsignedAtMost(index, last)) { return false; }
    tile.Indices.push_back(index.get<uint32_t>());
  }
  return true;
}
}

std::expected<SurfaceTile, std::string> ReadSurfaceTile(std::string_view path) {
  const auto source = ReadTextFile(path, kFileBytesMost);
  if (!source) { return std::unexpected(source.error()); }
  const auto record = Json::parse(*source, nullptr, false);
  SurfaceTile tile;
  tile.StoredBytes = source->size();
  if (!record.is_object() || !ReadHeader(record, tile) || !ReadMesh(record, tile)) {
    return std::unexpected("invalid surface tile header or mesh");
  }
  return tile;
}
}
