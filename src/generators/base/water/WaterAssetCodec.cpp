#include "WaterAsset.h"
#include "WaterAssetFormat.h"
#include "WaterSurfaceMeshCodec.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
constexpr uint64_t kLegacyVersion = 0x345245544157ULL;
constexpr uint64_t kSourceVersion = 0x355245544157ULL;
constexpr uint64_t kVersion = 0x365245544157ULL;

uint64_t NativeVersion(const WaterAsset::SurfaceGeometry *geometry) {
  if (geometry == nullptr) { return kLegacyVersion; }
  if (!geometry->ProfileDeclines.empty()) { return kVersion; }
  return geometry->FaceSourceOffsets.empty() ? kLegacyVersion : kSourceVersion;
}

bool Rejections(auto &archive, auto &rejections) {
  return WaterFormat::Records<2 * sizeof(uint32_t)>(
      archive, rejections, [](auto &out, auto &one) { return out(one.Surface, one.Reason); });
}

bool ArchiveGeometry(BinaryValueWriter &out,
                     const WaterAsset::SurfaceGeometry *geometry,
                     uint64_t version) {
  return out(geometry != nullptr) &&
         (geometry == nullptr ||
          (WaterSurfaceMeshCodec::Fields(out, geometry->Mesh) &&
           Rejections(out, geometry->Rejections) &&
           (version == kLegacyVersion || (out.Array<uint32_t>(geometry->FaceSourceOffsets) &&
                                          out.Array<uint32_t>(geometry->FaceSources))) &&
           (version != kVersion || Rejections(out, geometry->ProfileDeclines))));
}

bool ArchiveGeometry(BinaryValueReader &in,
                     std::optional<WaterAsset::SurfaceGeometry> &geometry,
                     uint64_t version) {
  bool present = false;
  if (!in(present)) { return false; }
  if (!present) { return true; }
  geometry.emplace();
  return WaterSurfaceMeshCodec::Fields(in, geometry->Mesh) &&
         Rejections(in, geometry->Rejections) &&
         (version == kLegacyVersion ||
          (in.Array(geometry->FaceSourceOffsets) && in.Array(geometry->FaceSources))) &&
         (version != kVersion || Rejections(in, geometry->ProfileDeclines));
}

bool ArchiveSurfaces(auto &archive, auto &surfaces) {
  return WaterFormat::Records<16>(archive, surfaces, [](auto &out, auto &surface) {
    return out(surface.FirstRing, surface.RingCount, surface.LevelM, surface.Input);
  });
}

bool ArchiveRings(auto &archive, auto &rings) {
  return WaterFormat::Records<8>(
      archive, rings, [](auto &out, auto &ring) { return out(ring.FirstPoint, ring.PointCount); });
}

bool ArchiveCourses(auto &archive, auto &courses) {
  return WaterFormat::Records<4 * sizeof(uint32_t) + sizeof(float)>(
      archive, courses, [](auto &out, auto &course) {
        return out(course.FirstPoint,
                   course.PointCount,
                   course.FirstLevel,
                   course.HalfWidthM,
                   course.Input);
      });
}

bool Within(size_t first, size_t count, size_t end) noexcept {
  return first <= end && count <= end - first;
}

bool ArchiveTiles(auto &archive, auto &tiles) {
  return WaterFormat::Records<3 * sizeof(int32_t) + 2 * sizeof(uint32_t)>(
      archive, tiles, [](auto &out, auto &tile) {
        return out(tile.Zoom, tile.X, tile.Y, tile.FirstSurface, tile.SurfaceCount);
      });
}

bool ValidTile(const WaterAsset::Tile &tile, size_t surfaces) {
  if (tile.Zoom < 0 || tile.Zoom > std::numeric_limits<int32_t>::digits) { return false; }
  const auto across = static_cast<int64_t>(uint64_t{1} << static_cast<unsigned>(tile.Zoom));
  return tile.X >= 0 && tile.Y >= 0 && tile.X < across && tile.Y < across &&
         Within(tile.FirstSurface, tile.SurfaceCount, surfaces);
}

bool Valid(const WaterAsset::Data &data) {
  const size_t points = data.Points.size() / 2;
  const bool scalars =
      data.Points.size() % 2 == 0 && data.NoGround >= 0 && data.Outliers >= 0 &&
      data.InvalidBodies >= 0 && data.ProcessedTiles <= data.Tiles.size() &&
      std::ranges::all_of(data.Points, [](double value) { return std::isfinite(value); }) &&
      std::ranges::all_of(data.Levels, [](float value) { return std::isfinite(value); });
  const bool rings = std::ranges::all_of(data.Rings, [points](const auto &ring) {
    return ring.PointCount >= 3 && Within(ring.FirstPoint, ring.PointCount, points);
  });
  const bool surfaces = std::ranges::all_of(data.Surfaces, [&](const auto &surface) {
    return surface.RingCount > 0 && std::isfinite(surface.LevelM) &&
           Within(surface.FirstRing, surface.RingCount, data.Rings.size());
  });
  const bool courses = std::ranges::all_of(data.Courses, [&](const auto &course) {
    return course.PointCount >= 2 && std::isfinite(course.HalfWidthM) && course.HalfWidthM > 0 &&
           Within(course.FirstPoint, course.PointCount, points) &&
           Within(course.FirstLevel, course.PointCount, data.Levels.size());
  });
  const bool tiles = std::ranges::all_of(
      data.Tiles, [&](const auto &tile) { return ValidTile(tile, data.Surfaces.size()); });
  return scalars && rings && surfaces && courses && tiles && WaterFormat::ValidMetadata(data);
}
}

std::expected<std::vector<uint8_t>, std::string> WaterAsset::EncodeNative(size_t bytesMost) const {
  const auto &data = Contents();
  if (!Complete() || !Valid(data)) {
    return std::unexpected("water asset is incomplete or invalid");
  }
  BinaryValueWriter out(bytesMost);
  const auto *geometry = PreparedSurfaces();
  const auto version = NativeVersion(geometry);
  if (!out(version,
           data.ProcessedTiles,
           static_cast<int64_t>(data.NoGround),
           static_cast<int64_t>(data.Outliers),
           static_cast<int64_t>(data.InvalidBodies)) ||
      !ArchiveSurfaces(out, data.Surfaces) || !ArchiveRings(out, data.Rings) ||
      !ArchiveCourses(out, data.Courses) || !out.Array<float>(data.Levels) ||
      !out.Array<double>(data.Points) || !ArchiveTiles(out, data.Tiles) ||
      !WaterFormat::Metadata(out, data) || !ArchiveGeometry(out, geometry, version)) {
    return std::unexpected("water asset exceeds its encoding budget");
  }
  return std::move(out.Out).TakeBytes();
}

std::expected<WaterAsset, std::string> WaterAsset::DecodeNative(std::span<const uint8_t> bytes) {
  BinaryValueReader in(bytes);
  uint64_t version = 0;
  int64_t noGround = 0;
  int64_t outliers = 0;
  int64_t invalidBodies = 0;
  Data data;
  std::optional<SurfaceGeometry> geometry;
  if (!in(version, data.ProcessedTiles, noGround, outliers, invalidBodies) ||
      (version != kVersion && version != kSourceVersion && version != kLegacyVersion) ||
      noGround < 0 || noGround > std::numeric_limits<long>::max() || outliers < 0 ||
      outliers > std::numeric_limits<long>::max() || invalidBodies < 0 ||
      invalidBodies > std::numeric_limits<long>::max() || !ArchiveSurfaces(in, data.Surfaces) ||
      !ArchiveRings(in, data.Rings) || !ArchiveCourses(in, data.Courses) ||
      !in.Array(data.Levels) || !in.Array(data.Points) || !ArchiveTiles(in, data.Tiles) ||
      !WaterFormat::Metadata(in, data) || !ArchiveGeometry(in, geometry, version) ||
      in.In.Remaining() != 0) {
    return std::unexpected("invalid native water asset");
  }
  data.NoGround = static_cast<long>(noGround);
  data.Outliers = static_cast<long>(outliers);
  data.InvalidBodies = static_cast<long>(invalidBodies);
  if (!Valid(data)) { return std::unexpected("native water asset ranges or scalars are invalid"); }
  WaterAsset asset(std::move(data));
  return geometry ? asset.WithSurfaces(std::move(*geometry))
                  : std::expected<WaterAsset, std::string>(std::move(asset));
}
}
