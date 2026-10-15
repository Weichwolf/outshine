#include "ClassRegionCompiler.h"
#include "GeosGeometry.h"
#include <geos_c.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
using Rasterizer = ClassificationRasterizer;
using Spatial::GeosGeometry;

struct Context {
  GEOSContextHandle_t Handle = GEOS_init_r();
  Context() = default;
  Context(const Context &) = delete;
  Context &operator=(const Context &) = delete;

  ~Context() {
    if (Handle != nullptr) { GEOS_finish_r(Handle); }
  }
};

GeosGeometry RingGeometry(GEOSContextHandle_t context,
                          const Rasterizer::Input &input,
                          const Rasterizer::Ring &ring) {
  if (ring.Count < 3 || ring.Count == std::numeric_limits<uint32_t>::max() ||
      static_cast<uint64_t>(ring.First) + ring.Count > input.Points.size() / 2) {
    return {nullptr, {context}};
  }
  auto *coordinates = GEOSCoordSeq_create_r(context, ring.Count + 1, 2);
  if (coordinates == nullptr) { return {nullptr, {context}}; }
  for (uint32_t at = 0; at <= ring.Count; ++at) {
    const size_t index = (static_cast<size_t>(ring.First) + at % ring.Count) * 2;
    if (!std::isfinite(input.Points[index]) || !std::isfinite(input.Points[index + 1]) ||
        GEOSCoordSeq_setXY_r(
            context, coordinates, at, input.Points[index], input.Points[index + 1]) == 0) {
      GEOSCoordSeq_destroy_r(context, coordinates);
      return {nullptr, {context}};
    }
  }
  return {GEOSGeom_createLinearRing_r(context, coordinates), {context}};
}

GeosGeometry Collection(GEOSContextHandle_t context, int type, std::vector<GeosGeometry> parts) {
  std::vector<GEOSGeometry *> raw;
  raw.reserve(parts.size());
  for (auto &part : parts) { raw.push_back(part.release()); }
  return {GEOSGeom_createCollection_r(context, type, raw.data(), static_cast<unsigned>(raw.size())),
          {context}};
}

GeosGeometry
Polygon(GEOSContextHandle_t context, GeosGeometry shell, std::vector<GeosGeometry> holes) {
  std::vector<GEOSGeometry *> raw;
  raw.reserve(holes.size());
  for (auto &hole : holes) { raw.push_back(hole.release()); }
  return {GEOSGeom_createPolygon_r(
              context, shell.release(), raw.data(), static_cast<unsigned>(raw.size())),
          {context}};
}

GeosGeometry FeatureGeometry(GEOSContextHandle_t context,
                             const Rasterizer::Input &input,
                             const Rasterizer::Feature &feature) {
  if (feature.RingCount == 0 ||
      static_cast<uint64_t>(feature.FirstRing) + feature.RingCount > input.Rings.size()) {
    return {nullptr, {context}};
  }
  std::vector<GeosGeometry> polygons;
  size_t at = feature.FirstRing;
  const size_t end = at + feature.RingCount;
  while (at < end) {
    const auto &outer = input.Rings[at];
    if (outer.Role == Rasterizer::RingRole::Hole ||
        (feature.RingCount > 1 && outer.Role == Rasterizer::RingRole::Unspecified)) {
      return {nullptr, {context}};
    }
    auto shell = RingGeometry(context, input, outer);
    if (!shell) { return {nullptr, {context}}; }
    ++at;
    std::vector<GeosGeometry> holes;
    while (at < end && input.Rings[at].Role == Rasterizer::RingRole::Hole) {
      holes.push_back(RingGeometry(context, input, input.Rings[at++]));
      if (!holes.back()) { return {nullptr, {context}}; }
    }
    auto polygon = Polygon(context, std::move(shell), std::move(holes));
    if (!polygon || GEOSisValid_r(context, polygon.get()) != 1) { return {nullptr, {context}}; }
    polygons.push_back(std::move(polygon));
  }
  auto result = Collection(context, GEOS_MULTIPOLYGON, std::move(polygons));
  if (!result || GEOSisValid_r(context, result.get()) != 1) { return {nullptr, {context}}; }
  return result;
}

bool CopyFeature(const Rasterizer::Input &input,
                 const Rasterizer::Feature &feature,
                 Rasterizer::Input &output) {
  if (static_cast<uint64_t>(feature.FirstRing) + feature.RingCount > input.Rings.size()) {
    return false;
  }
  if (output.Rings.size() + feature.RingCount > std::numeric_limits<uint32_t>::max()) {
    return false;
  }
  auto copied = feature;
  copied.FirstRing = static_cast<uint32_t>(output.Rings.size());
  for (size_t at = feature.FirstRing;
       at < static_cast<size_t>(feature.FirstRing) + feature.RingCount;
       ++at) {
    auto ring = input.Rings[at];
    if (static_cast<uint64_t>(ring.First) + ring.Count > input.Points.size() / 2 ||
        output.Points.size() / 2 + ring.Count > std::numeric_limits<uint32_t>::max()) {
      return false;
    }
    const auto first = input.Points.begin() + static_cast<ptrdiff_t>(ring.First) * 2;
    ring.First = static_cast<uint32_t>(output.Points.size() / 2);
    output.Points.insert(
        output.Points.end(), first, first + static_cast<ptrdiff_t>(ring.Count) * 2);
    output.Rings.push_back(ring);
  }
  output.Features.push_back(copied);
  return true;
}

bool AppendRing(GEOSContextHandle_t context,
                const GEOSGeometry *geometry,
                Rasterizer::RingRole role,
                Rasterizer::Input &output,
                Rasterizer::Feature &feature) {
  const auto *coordinates = GEOSGeom_getCoordSeq_r(context, geometry);
  unsigned count = 0;
  if (coordinates == nullptr || GEOSCoordSeq_getSize_r(context, coordinates, &count) == 0 ||
      count < 4 || output.Points.size() / 2 + count - 1 > std::numeric_limits<uint32_t>::max() ||
      output.Rings.size() == std::numeric_limits<uint32_t>::max() ||
      feature.RingCount == std::numeric_limits<uint32_t>::max()) {
    return false;
  }
  const Rasterizer::Ring ring{
      .First = static_cast<uint32_t>(output.Points.size() / 2), .Count = count - 1, .Role = role};
  for (unsigned at = 0; at + 1 < count; ++at) {
    double east = 0;
    double north = 0;
    if (GEOSCoordSeq_getXY_r(context, coordinates, at, &east, &north) == 0 ||
        !std::isfinite(east) || !std::isfinite(north) ||
        std::abs(east) > std::numeric_limits<float>::max() ||
        std::abs(north) > std::numeric_limits<float>::max()) {
      return false;
    }
    const auto e = static_cast<float>(east);
    const auto n = static_cast<float>(north);
    output.Points.push_back(e);
    output.Points.push_back(n);
    feature.MinE = std::min(feature.MinE, e);
    feature.MaxE = std::max(feature.MaxE, e);
    feature.MinN = std::min(feature.MinN, n);
    feature.MaxN = std::max(feature.MaxN, n);
  }
  output.Rings.push_back(ring);
  ++feature.RingCount;
  return true;
}

bool AppendGeometry(GEOSContextHandle_t context,
                    const GEOSGeometry *geometry,
                    Rasterizer::Input &output,
                    Rasterizer::Feature &feature) {
  const int type = GEOSGeomTypeId_r(context, geometry);
  if (type == GEOS_POLYGON) {
    if (!AppendRing(context,
                    GEOSGetExteriorRing_r(context, geometry),
                    Rasterizer::RingRole::Exterior,
                    output,
                    feature)) {
      return false;
    }
    const int holes = GEOSGetNumInteriorRings_r(context, geometry);
    if (holes < 0) { return false; }
    for (int at = 0; at < holes; ++at) {
      if (!AppendRing(context,
                      GEOSGetInteriorRingN_r(context, geometry, at),
                      Rasterizer::RingRole::Hole,
                      output,
                      feature)) {
        return false;
      }
    }
    return true;
  }
  if (type != GEOS_MULTIPOLYGON) { return false; }
  const int count = GEOSGetNumGeometries_r(context, geometry);
  if (count < 1) { return false; }
  for (int at = 0; at < count; ++at) {
    if (!AppendGeometry(context, GEOSGetGeometryN_r(context, geometry, at), output, feature)) {
      return false;
    }
  }
  return true;
}

bool MergeGroup(GEOSContextHandle_t context,
                const Rasterizer::Input &input,
                std::span<const uint32_t> group,
                Rasterizer::Input &output,
                const std::stop_token &stop) {
  std::vector<GeosGeometry> polygons;
  for (const auto index : group) {
    if (stop.stop_requested()) { return false; }
    polygons.push_back(FeatureGeometry(context, input, input.Features[index]));
    if (!polygons.back()) { return false; }
  }
  auto collection = Collection(context, GEOS_GEOMETRYCOLLECTION, std::move(polygons));
  if (!collection) { return false; }
  GeosGeometry merged{GEOSUnaryUnion_r(context, collection.get()), {context}};
  if (!merged || stop.stop_requested() || GEOSisValid_r(context, merged.get()) != 1) {
    return false;
  }
  auto feature = input.Features[group.front()];
  feature.FirstRing = static_cast<uint32_t>(output.Rings.size());
  feature.RingCount = 0;
  feature.MinE = feature.MinN = std::numeric_limits<float>::max();
  feature.MaxE = feature.MaxN = std::numeric_limits<float>::lowest();
  const auto pointsBefore = output.Points.size();
  const auto ringsBefore = output.Rings.size();
  if (!AppendGeometry(context, merged.get(), output, feature)) {
    output.Points.resize(pointsBefore);
    output.Rings.resize(ringsBefore);
    return false;
  }
  output.Features.push_back(feature);
  return true;
}

std::vector<std::vector<uint32_t>> Groups(const Rasterizer::Input &source) {
  std::map<int, int> rankRows;
  for (const auto &feature : source.Features) {
    auto [entry, added] = rankRows.try_emplace(feature.Rank, feature.ClassRow);
    if (!added && std::cmp_not_equal(entry->second, feature.ClassRow)) { entry->second = -1; }
  }
  std::map<std::pair<int, int64_t>, size_t> indices;
  std::vector<std::vector<uint32_t>> groups;
  for (uint32_t index = 0; index < source.Features.size(); ++index) {
    const auto &feature = source.Features[index];
    const bool merge = feature.Form == Rasterizer::Shape::Polygon && rankRows[feature.Rank] >= 0;
    const auto key = std::pair{feature.Rank,
                               merge ? static_cast<int64_t>(feature.ClassRow)
                                     : -1 - static_cast<int64_t>(index)};
    auto [entry, added] = indices.try_emplace(key, groups.size());
    if (added) { groups.emplace_back(); }
    groups[entry->second].push_back(index);
  }
  return groups;
}
}

std::optional<CompiledClassRegions>
CompileClassRegions(const ClassificationRasterizer::Input &source, const std::stop_token &stop) {
  if (stop.stop_requested() || source.Features.size() > std::numeric_limits<uint32_t>::max() ||
      source.Rings.size() > std::numeric_limits<uint32_t>::max() || source.Points.size() % 2 != 0 ||
      source.Points.size() / 2 > std::numeric_limits<uint32_t>::max()) {
    return std::nullopt;
  }
  CompiledClassRegions output;
  output.Raster.CamE = source.CamE;
  output.Raster.CamN = source.CamN;
  output.Raster.CellM = source.CellM;
  output.Raster.BoundaryHaloM = source.BoundaryHaloM;
  output.Raster.HalfCells = source.HalfCells;
  const Context context;
  for (const auto &group : Groups(source)) {
    if (stop.stop_requested()) { return std::nullopt; }
    if (group.size() > 1 && context.Handle != nullptr &&
        MergeGroup(context.Handle, source, group, output.Raster, stop)) {
      output.Contributors.push_back(group);
      ++output.CoalescedGroups;
    } else {
      ++output.RetainedGroups;
      for (const auto index : group) {
        if (!CopyFeature(source, source.Features[index], output.Raster)) { return std::nullopt; }
        output.Contributors.push_back({index});
      }
    }
  }
  return output;
}
}
