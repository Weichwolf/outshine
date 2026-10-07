#include "WaterField.h"
#include "BinaryValueArchive.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Generators::Osm {
namespace {
constexpr uint64_t kVersion = 0x315245544157ULL;

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
}

std::expected<std::vector<uint8_t>, std::string> WaterField::EncodeNative(const OsmField &source,
                                                                          size_t bytesMost) const {
  if (!Ingested(source)) { return std::unexpected("water inputs are incomplete"); }
  BinaryValueWriter out(bytesMost);
  if (source.Tiles().size() > std::numeric_limits<uint32_t>::max() ||
      !out(kVersion,
           static_cast<uint64_t>(source.Points().size()),
           static_cast<uint64_t>(source.Tiles().size()),
           static_cast<int64_t>(NoGround_),
           static_cast<int64_t>(Outliers_),
           static_cast<int64_t>(InvalidBodies_)) ||
      !ArchiveSurfaces(out, Surfaces_) || !ArchiveRings(out, SurfaceRings_) ||
      !ArchiveCourses(out, Courses_) || !out.Array<float>(Levels_)) {
    return std::unexpected("water inputs exceed their encoding budget");
  }
  for (uint32_t tile = 0; tile < source.Tiles().size(); ++tile) {
    const auto range = ByTile_.At(tile);
    if (!out(range.First, range.Count)) {
      return std::unexpected("water tile ranges exceed their encoding budget");
    }
  }
  return std::move(out.Out).TakeBytes();
}

std::expected<WaterField, std::string> WaterField::DecodeNative(std::span<const uint8_t> bytes,
                                                                const OsmField &source) {
  BinaryValueReader in(bytes);
  uint64_t version = 0;
  uint64_t points = 0;
  uint64_t tiles = 0;
  int64_t noGround = 0;
  int64_t outliers = 0;
  int64_t invalidBodies = 0;
  WaterField water;
  if (!in(version, points, tiles, noGround, outliers, invalidBodies) || version != kVersion ||
      points != source.Points().size() || tiles != source.Tiles().size() ||
      tiles > std::numeric_limits<uint32_t>::max() || noGround < 0 || outliers < 0 ||
      invalidBodies < 0 || noGround > std::numeric_limits<long>::max() ||
      outliers > std::numeric_limits<long>::max() ||
      invalidBodies > std::numeric_limits<long>::max() || !ArchiveSurfaces(in, water.Surfaces_) ||
      !ArchiveRings(in, water.SurfaceRings_) || !ArchiveCourses(in, water.Courses_) ||
      !in.Array(water.Levels_)) {
    return std::unexpected("invalid native water inputs");
  }
  const auto allFinite =
      std::ranges::all_of(water.Levels_, [](float value) { return std::isfinite(value); });
  if (!allFinite ||
      !std::ranges::all_of(water.SurfaceRings_,
                           [points](const auto &ring) {
                             return ring.PointCount >= 3 &&
                                    Within(ring.FirstPoint, ring.PointCount, points / 2);
                           }) ||
      !std::ranges::all_of(
          water.Surfaces_,
          [&](const auto &surface) {
            return surface.RingCount > 0 && std::isfinite(surface.LevelM) &&
                   Within(surface.FirstRing, surface.RingCount, water.SurfaceRings_.size());
          }) ||
      !std::ranges::all_of(water.Courses_, [&](const auto &course) {
        return course.PointCount >= 2 && std::isfinite(course.HalfWidthM) &&
               course.HalfWidthM > 0 && Within(course.FirstPoint, course.PointCount, points / 2) &&
               Within(course.FirstLevel, course.PointCount, water.Levels_.size());
      })) {
    return std::unexpected("native water ranges or levels are invalid");
  }
  for (uint32_t tile = 0; tile < tiles; ++tile) {
    uint32_t first = 0;
    uint32_t count = 0;
    if (!in(first, count) || !Within(first, count, water.Surfaces_.size())) {
      return std::unexpected("invalid native water tile range");
    }
    water.ByTile_.Set(tile, first, first + count);
  }
  if (in.In.Remaining() != 0) { return std::unexpected("trailing native water bytes"); }
  std::vector<uint8_t> taken(source.Tiles().size());
  for (const auto &feature : source.Features()) {
    if (feature.Tile >= taken.size()) {
      return std::unexpected("invalid native water source tile");
    }
    if (taken[feature.Tile] == 0) {
      water.Mark_.Take(feature.Tile);
      taken[feature.Tile] = 1;
    }
  }
  water.Mark_.Advance(source.Features());
  water.NoGround_ = static_cast<long>(noGround);
  water.Outliers_ = static_cast<long>(outliers);
  water.InvalidBodies_ = static_cast<long>(invalidBodies);
  water.SourceGeneration_ = source.Generation();
  return water;
}
}
