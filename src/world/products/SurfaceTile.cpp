#include "SurfaceTile.h"
#include "ByteArchive.h"
#include "ReadTextFile.h"
#include "Units.h"
#include <algorithm>
#include <bit>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <nlohmann/json.hpp>

namespace outshine {
namespace {
using Json = nlohmann::json;
constexpr size_t kFileBytesMost = size_t{8} * 1024 * 1024;
constexpr size_t kRevisionLength = 64;
constexpr double kCoordinateLimitM = 2e7;
constexpr std::string_view kPackedMagic = "OUTSHST1";
constexpr std::string_view kGridMagic = "OUTSHST2";
constexpr std::string_view kPaintedMagic = "OUTSHST3";
constexpr uint32_t kMaximumGridSide = 256;

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

bool InRange(double value, double low, double high) {
  return value >= low && value <= high;
}

bool ValidGrid(const SurfaceTile &tile, size_t vertices, bool painted = false) {
  const size_t side = tile.GridSide;
  if (side == 0) { return !painted; }
  return side >= 2 && side <= kMaximumGridSide && std::has_single_bit(side - 1) &&
         (painted ? side * side <= vertices : side * side == vertices);
}

bool ValidMetadata(const SurfaceTile &tile) {
  if (tile.Coverage.Zoom < 0 || tile.Coverage.Zoom > Data::TileId::MaximumZoom) { return false; }
  const auto last = (1u << static_cast<unsigned>(tile.Coverage.Zoom)) - 1;
  if (tile.Coverage.X > last || tile.Coverage.Y > last ||
      !InRange(tile.Origin.LongitudeDeg, -kDegPerHalfTurn, kDegPerHalfTurn) ||
      !InRange(tile.Origin.LatitudeDeg, -kDegPerHalfTurn / 2, kDegPerHalfTurn / 2) ||
      !InRange(tile.GeometricErrorM, 0, kCoordinateLimitM) ||
      tile.Revision.size() != kRevisionLength ||
      !std::ranges::all_of(tile.Revision,
                           [](unsigned char value) { return std::isxdigit(value); })) {
    return false;
  }
  for (size_t axis = 0; axis < 3; ++axis) {
    if (!InRange(tile.MinimumM[axis], -kCoordinateLimitM, tile.MaximumM[axis]) ||
        tile.MaximumM[axis] > kCoordinateLimitM) {
      return false;
    }
  }
  return true;
}

bool ValidVertex(const SurfaceVertex &vertex, const SurfaceTile &tile) {
  constexpr float kNormalSquaredTolerance = 0.001F;
  if (!(std::abs(Dot(vertex.Normal, vertex.Normal) - 1) <= kNormalSquaredTolerance)) {
    return false;
  }
  for (size_t axis = 0; axis < 3; ++axis) {
    if (!InRange(vertex.PositionM[axis], tile.MinimumM[axis], tile.MaximumM[axis])) {
      return false;
    }
  }
  return true;
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
  if (const auto grid = record.find("grid_side"); grid != record.end()) {
    if (!UnsignedAtMost(*grid, kMaximumGridSide)) { return false; }
    tile.GridSide = grid->get<uint32_t>();
  }
  return ValidMetadata(tile);
}

bool ReadMesh(const Json &record, SurfaceTile &tile) {
  if (!record.contains("vertices_eus_m") || !record.contains("normals_eus") ||
      !record.contains("indices")) {
    return false;
  }
  const auto &vertices = record["vertices_eus_m"];
  const auto &normals = record["normals_eus"];
  const auto &indices = record["indices"];
  if (!vertices.is_array() || !normals.is_array() || normals.size() != vertices.size() ||
      vertices.size() < 3 || vertices.size() > SurfaceTile::MaximumVertices ||
      !indices.is_array() || indices.empty() || indices.size() % 3 != 0 ||
      indices.size() > SurfaceTile::MaximumIndices || !ValidGrid(tile, vertices.size())) {
    return false;
  }
  tile.Vertices.reserve(vertices.size());
  tile.Indices.reserve(indices.size());
  for (size_t at = 0; at < vertices.size(); ++at) {
    if (!Numbers(vertices[at], 3) || !Numbers(normals[at], 3)) { return false; }
    const auto position = Triple(vertices[at]);
    const auto normal = Triple(normals[at]);
    SurfaceVertex packed;
    for (size_t axis = 0; axis < 3; ++axis) {
      packed.PositionM[axis] = static_cast<float>(position[axis]);
      packed.Normal[axis] = static_cast<float>(normal[axis]);
    }
    if (!ValidVertex(packed, tile)) { return false; }
    tile.Vertices.push_back(packed);
  }
  const auto last = static_cast<uint32_t>(vertices.size() - 1);
  for (const auto &index : indices) {
    if (!UnsignedAtMost(index, last)) { return false; }
    tile.Indices.push_back(index.get<uint32_t>());
  }
  tile.GroundIndices = static_cast<uint32_t>(tile.Indices.size());
  return true;
}

bool ReadPackedMetadata(ByteReader &input, SurfaceTile &tile) {
  uint32_t zoom = 0;
  if (!input.Number(zoom) || zoom > Data::TileId::MaximumZoom || !input.Number(tile.Coverage.X) ||
      !input.Number(tile.Coverage.Y) || !input.Number(tile.Origin.LongitudeDeg) ||
      !input.Number(tile.Origin.LatitudeDeg)) {
    return false;
  }
  tile.Coverage.Zoom = static_cast<int>(zoom);
  const auto revision = input.Take(kRevisionLength);
  if (!revision) { return false; }
  tile.Revision.assign(revision->begin(), revision->end());
  for (auto *const limit : {&tile.MinimumM, &tile.MaximumM}) {
    for (auto &coordinate : limit->Axis) {
      if (!input.Number(coordinate)) { return false; }
    }
  }
  return input.Number(tile.GeometricErrorM) && ValidMetadata(tile);
}

bool ReadPackedVertices(ByteReader &input, std::span<SurfaceVertex> vertices) {
  for (auto &vertex : vertices) {
    for (auto *const attribute : {&vertex.PositionM, &vertex.Normal}) {
      for (auto &value : attribute->Axis) {
        if (!input.Number(value)) { return false; }
      }
    }
  }
  return true;
}

bool ReadPackedMesh(ByteReader &input, SurfaceTile &tile, bool hasGrid, bool painted) {
  uint32_t vertices = 0;
  uint32_t indices = 0;
  if (!input.Number(vertices) || !input.Number(indices) || vertices < 3 ||
      vertices > SurfaceTile::MaximumVertices || indices == 0 ||
      indices > SurfaceTile::MaximumIndices || indices % 3 != 0) {
    return false;
  }
  if ((hasGrid && !input.Number(tile.GridSide)) || !ValidGrid(tile, vertices, painted)) {
    return false;
  }
  tile.GroundIndices = indices;
  if (painted && (!input.Number(tile.GroundIndices) || tile.GroundIndices == 0 ||
                  tile.GroundIndices > indices || tile.GroundIndices % 3 != 0)) {
    return false;
  }
  const size_t groundVertices = painted ? tile.GridSide * tile.GridSide : vertices;
  constexpr size_t kVertexBytes = 6 * sizeof(float);
  const auto vertexData = input.Take(vertices * kVertexBytes);
  const auto indexData = input.Take(indices * sizeof(uint32_t));
  const auto colourData = input.Take((vertices - groundVertices) * sizeof(SurfaceColour));
  if (!vertexData || !indexData || !colourData || input.Remaining() != 0) { return false; }
  tile.Vertices.resize(vertices);
  tile.Indices.resize(indices);
  tile.Colours.resize(vertices - groundVertices);
  if (!tile.Colours.empty()) {
    std::memcpy(tile.Colours.data(), colourData->data(), colourData->size());
  }
  if (std::endian::native == std::endian::little && sizeof(SurfaceVertex) == kVertexBytes) {
    std::memcpy(tile.Vertices.data(), vertexData->data(), vertexData->size());
    std::memcpy(tile.Indices.data(), indexData->data(), indexData->size());
  } else {
    ByteReader packedVertices(*vertexData);
    ByteReader packedIndices(*indexData);
    if (!ReadPackedVertices(packedVertices, tile.Vertices)) { return false; }
    for (auto &index : tile.Indices) {
      if (!packedIndices.Number(index)) { return false; }
    }
  }
  const auto allIndices = std::span(tile.Indices);
  const auto ground = allIndices.first(tile.GroundIndices);
  const auto overlay = allIndices.subspan(tile.GroundIndices);
  return std::ranges::all_of(tile.Vertices, [&](const auto &v) { return ValidVertex(v, tile); }) &&
         std::ranges::all_of(ground,
                             [groundVertices](uint32_t index) { return index < groundVertices; }) &&
         std::ranges::all_of(
             overlay,
             [=](uint32_t index) { return index >= groundVertices && index < vertices; }) &&
         (tile.Colours.empty() == overlay.empty()) &&
         std::ranges::all_of(
             tile.Colours, [](const SurfaceColour &colour) { return colour.back() == UINT8_MAX; });
}
}

std::expected<SurfaceTile, std::string> DecodePackedSurfaceTile(std::span<const uint8_t> bytes) {
  ByteReader input(bytes);
  const auto magic = input.Take(kPackedMagic.size());
  SurfaceTile tile;
  tile.StoredBytes = bytes.size();
  if (!magic) { return std::unexpected("invalid packed surface tile header"); }
  const bool painted = std::ranges::equal(*magic, kPaintedMagic);
  const bool hasGrid = painted || std::ranges::equal(*magic, kGridMagic);
  if ((!hasGrid && !std::ranges::equal(*magic, kPackedMagic)) || !ReadPackedMetadata(input, tile) ||
      !ReadPackedMesh(input, tile, hasGrid, painted)) {
    return std::unexpected("invalid packed surface tile header or mesh");
  }
  return tile;
}

std::expected<SurfaceTile, std::string> ReadSurfaceTile(std::string_view path) {
  const auto source = ReadTextFile(path, kFileBytesMost);
  if (!source) { return std::unexpected(source.error()); }
  if (source->starts_with(kPackedMagic) || source->starts_with(kGridMagic) ||
      source->starts_with(kPaintedMagic)) {
    return DecodePackedSurfaceTile(
        {reinterpret_cast<const uint8_t *>(source->data()), source->size()});
  }
  const auto record = Json::parse(*source, nullptr, false);
  SurfaceTile tile;
  tile.StoredBytes = source->size();
  if (!record.is_object() || !ReadHeader(record, tile) || !ReadMesh(record, tile)) {
    return std::unexpected("invalid surface tile header or mesh");
  }
  return tile;
}
}
