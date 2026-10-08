#include "WaterAsset.h"
#include "BinaryValueArchive.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
constexpr uint64_t kVersion = 0x325245544157ULL;

template <class Archive, class T, class Fields>
bool Records(Archive &archive, std::vector<T> &records, Fields fields) {
  uint64_t count = 0;
  if (!archive(count) || count > archive.In.Remaining() / sizeof(T)) { return false; }
  records.resize(count);
  return std::ranges::all_of(records, [&](auto &record) { return fields(archive, record); });
}

template <class T, class Fields>
bool Records(BinaryValueWriter &archive, const std::vector<T> &records, Fields fields) {
  return archive(static_cast<uint64_t>(records.size())) &&
         std::ranges::all_of(records, [&](const auto &record) { return fields(archive, record); });
}

bool ArchiveSurfaces(auto &archive, auto &surfaces) {
  return Records(archive, surfaces, [](auto &out, auto &surface) {
    return out(surface.FirstRing, surface.RingCount, surface.LevelM);
  });
}

bool ArchiveRings(auto &archive, auto &rings) {
  return Records(
      archive, rings, [](auto &out, auto &ring) { return out(ring.FirstPoint, ring.PointCount); });
}

bool ArchiveCourses(auto &archive, auto &courses) {
  return Records(archive, courses, [](auto &out, auto &course) {
    return out(course.FirstPoint, course.PointCount, course.FirstLevel, course.HalfWidthM);
  });
}

bool Within(size_t first, size_t count, size_t end) noexcept {
  return first <= end && count <= end - first;
}

bool ArchiveTiles(auto &archive, auto &tiles) {
  return Records(archive, tiles, [](auto &out, auto &tile) {
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
  return scalars && rings && surfaces && courses && tiles;
}
}

std::expected<std::vector<uint8_t>, std::string> WaterAsset::EncodeNative(size_t bytesMost) const {
  const auto &data = Contents();
  if (!Complete() || !Valid(data)) {
    return std::unexpected("water asset is incomplete or invalid");
  }
  BinaryValueWriter out(bytesMost);
  if (!out(kVersion,
           data.ProcessedTiles,
           static_cast<int64_t>(data.NoGround),
           static_cast<int64_t>(data.Outliers),
           static_cast<int64_t>(data.InvalidBodies)) ||
      !ArchiveSurfaces(out, data.Surfaces) || !ArchiveRings(out, data.Rings) ||
      !ArchiveCourses(out, data.Courses) || !out.Array<float>(data.Levels) ||
      !out.Array<double>(data.Points) || !ArchiveTiles(out, data.Tiles)) {
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
  if (!in(version, data.ProcessedTiles, noGround, outliers, invalidBodies) || version != kVersion ||
      noGround < 0 || noGround > std::numeric_limits<long>::max() || outliers < 0 ||
      outliers > std::numeric_limits<long>::max() || invalidBodies < 0 ||
      invalidBodies > std::numeric_limits<long>::max() || !ArchiveSurfaces(in, data.Surfaces) ||
      !ArchiveRings(in, data.Rings) || !ArchiveCourses(in, data.Courses) ||
      !in.Array(data.Levels) || !in.Array(data.Points) || !ArchiveTiles(in, data.Tiles) ||
      in.In.Remaining() != 0) {
    return std::unexpected("invalid native water asset");
  }
  data.NoGround = static_cast<long>(noGround);
  data.Outliers = static_cast<long>(outliers);
  data.InvalidBodies = static_cast<long>(invalidBodies);
  if (!Valid(data)) { return std::unexpected("native water asset ranges or scalars are invalid"); }
  return WaterAsset(std::move(data));
}
}
