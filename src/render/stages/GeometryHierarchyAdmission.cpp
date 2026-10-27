#include "GeometryHierarchyAdmission.h"
#include "GpuPlacement.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <utility>

namespace outshine::Render {
namespace {
Native::Error InvalidHierarchy() {
  return {.Kind = Native::ErrorKind::Invalid,
          .Message = "geometry hierarchy exceeds its source, storage, integer or dispatch bounds"};
}

bool Fits(uint64_t count, uint64_t stride, const Native::Capabilities &limits) {
  return count <=
         std::min<uint64_t>(limits.MaxStorageBufferBytes, std::numeric_limits<uint32_t>::max()) /
             stride;
}

struct GridShape {
  uint32_t Width;
  uint32_t Lanes;
};

Native::Result<Native::Workgroups>
Grid(uint32_t groups, GridShape shape, const Native::Capabilities &limits) {
  const auto [width, lanes] = shape;
  const uint32_t rows = groups / width + uint32_t{groups % width != 0};
  const uint64_t padded = uint64_t{width} * rows;
  if (rows > limits.MaxWorkgroupCount[1] || limits.MaxWorkgroupCount[2] == 0 ||
      padded > std::numeric_limits<uint32_t>::max() / lanes) {
    return std::unexpected(InvalidHierarchy());
  }
  return Native::Workgroups{.X = std::min(groups, width), .Y = std::max(rows, 1u)};
}

bool SourcesFit(const GeometryHierarchyInputs &inputs, const Native::Capabilities &limits) {
  if (inputs.Roots.empty() || inputs.Templates.empty() ||
      !Fits(inputs.Roots.size(), sizeof(GeometryRoot), limits) ||
      !Fits(inputs.Roots.size() + 1, sizeof(uint32_t), limits) ||
      !Fits(inputs.Templates.size(), sizeof(GeometryClusterTemplate), limits) ||
      !Fits(inputs.SphereCount, size_t{3} * 4 * sizeof(float), limits) ||
      !Fits(inputs.PlacementCount, sizeof(GpuPlacement), limits) ||
      !Fits(inputs.SourceIndexCount, 4, limits) ||
      !Fits(inputs.BatchCount, sizeof(Native::IndexedIndirectArguments), limits)) {
    return false;
  }
  return std::ranges::all_of(inputs.Templates, [&](const auto &one) {
    return one.SphereIndex < inputs.SphereCount && one.IndexCount != 0 &&
           uint64_t{one.RelativeFirstIndex} + one.IndexCount <= inputs.SourceIndexCount;
  });
}

bool RootFits(const GeometryRoot &root,
              const GeometryHierarchyInputs &inputs,
              uint64_t logical,
              uint32_t previousBatch) {
  if (root.LogicalFirst != logical || root.BatchRow >= inputs.BatchCount ||
      root.BatchRow < previousBatch || root.ModelSlot >= inputs.PlacementCount ||
      uint64_t{root.TemplateFirst} + root.TemplateCount > inputs.Templates.size()) {
    return false;
  }
  uint64_t indexWords = 0;
  for (const auto &one : inputs.Templates.subspan(root.TemplateFirst, root.TemplateCount)) {
    indexWords += one.IndexCount;
    if (uint64_t{root.IndexBase} + one.RelativeFirstIndex + one.IndexCount >
        inputs.SourceIndexCount) {
      return false;
    }
  }
  return indexWords <= root.IndexCapacity;
}
}

Native::Result<GeometryHierarchyAdmission>
AdmitGeometryHierarchy(const GeometryHierarchyInputs &inputs, const Native::Capabilities &limits) {
  const uint32_t width = std::min(256u, limits.MaxWorkgroupCount[0]);
  if (width == 0 || limits.MaxWorkgroupSize[0] < kGeometryChunkClusters ||
      limits.MaxWorkgroupInvocations < kGeometryChunkClusters || !SourcesFit(inputs, limits)) {
    return std::unexpected(InvalidHierarchy());
  }
  GeometryHierarchyAdmission result;
  result.ChunkPrefix.reserve(inputs.Roots.size() + 1);
  result.ChunkPrefix.push_back(0);
  uint64_t logical = 0;
  uint64_t chunks = 0;
  uint64_t indices = 0;
  uint32_t previousBatch = 0;
  for (const auto &root : inputs.Roots) {
    if (!RootFits(root, inputs, logical, previousBatch)) {
      return std::unexpected(InvalidHierarchy());
    }
    logical += root.TemplateCount;
    chunks += root.TemplateCount / kGeometryChunkClusters +
              uint32_t{root.TemplateCount % kGeometryChunkClusters != 0};
    indices += root.IndexCapacity;
    if (!Fits(logical, 8, limits) || !Fits(chunks, 8, limits) || !Fits(indices, 4, limits)) {
      return std::unexpected(InvalidHierarchy());
    }
    result.ChunkPrefix.push_back(static_cast<uint32_t>(chunks));
    previousBatch = root.BatchRow;
  }
  const auto rootCount = static_cast<uint32_t>(inputs.Roots.size());
  const auto rootGroups =
      Grid(rootCount / 64 + uint32_t{rootCount % 64 != 0}, {.Width = width, .Lanes = 64}, limits);
  const auto scanGroups = Grid(rootCount, {.Width = width, .Lanes = 1}, limits);
  const auto leafGroups = Grid(static_cast<uint32_t>(chunks), {.Width = width, .Lanes = 1}, limits);
  const auto compactGroups =
      Grid(static_cast<uint32_t>(logical), {.Width = width, .Lanes = 1}, limits);
  if (!rootGroups || !scanGroups || !leafGroups || !compactGroups) {
    return std::unexpected(InvalidHierarchy());
  }
  result.View = {.RootCount = rootCount,
                 .TemplateCount = static_cast<uint32_t>(inputs.Templates.size()),
                 .ChunkCapacity = static_cast<uint32_t>(chunks),
                 .ClusterCapacity = static_cast<uint32_t>(logical),
                 .WorstChunks = static_cast<uint32_t>(chunks),
                 .GridWidth = width};
  result.RootGroups = *rootGroups;
  result.ScanGroups = *scanGroups;
  result.IndexWords = static_cast<uint32_t>(indices);
  return result;
}
}
