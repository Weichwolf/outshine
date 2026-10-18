#include "local_light_build.h"
#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <numeric>
#include <span>

namespace LightBuild {
void Memory::Reset() {
  Allocations = 0;
  AllocatedBytes = 0;
  PeakBytes = LiveBytes;
}

void *Memory::do_allocate(size_t bytes, size_t alignment) {
  ++Allocations;
  AllocatedBytes += bytes;
  LiveBytes += bytes;
  PeakBytes = std::max(PeakBytes, LiveBytes);
  return std::pmr::new_delete_resource()->allocate(bytes, alignment);
}

void Memory::do_deallocate(void *pointer, size_t bytes, size_t alignment) {
  LiveBytes -= bytes;
  std::pmr::new_delete_resource()->deallocate(pointer, bytes, alignment);
}

bool Memory::do_is_equal(const memory_resource &other) const noexcept {
  return this == &other;
}

void Builder::GatherBlock(const Camera &camera,
                          std::span<const double> edges,
                          uint32_t light,
                          const Bounds &cells,
                          const Vector &position,
                          double radius) {
  const uint32_t nx = (camera.Grid[0] + camera.Grid[3] - 1) / camera.Grid[3];
  const uint32_t ny = (camera.Grid[1] + camera.Grid[3] - 1) / camera.Grid[3];
  const int z = cells.First[2];
  for (int y = cells.First[1]; y <= cells.Last[1]; ++y) {
    for (int x = cells.First[0]; x <= cells.Last[0]; ++x) {
      CountWork(Tested);
      if (!Intersects(camera,
                      edges,
                      position,
                      radius,
                      {.First = {x, y, z}, .Last = {x, y, z}, .Empty = false})) {
        continue;
      }
      const auto cell = (static_cast<uint32_t>(z) * ny + static_cast<uint32_t>(y)) * nx +
                        static_cast<uint32_t>(x);
      ++Counts[cell];
      RetainGrowth(Pairs, Pairs.size() + 1);
      Pairs.emplace_back(cell, light);
    }
  }
}

void Builder::GatherLight(const Camera &camera,
                          std::span<const double> edges,
                          const Vector &position,
                          double radius,
                          Method method,
                          uint32_t light) {
  const Bounds bounds = Project(camera, edges, position, radius);
  if (bounds.Empty) { return; }
  if (method != Method::Hierarchy) {
    for (int z = bounds.First[2]; z <= bounds.Last[2]; ++z) {
      Bounds layer = bounds;
      layer.First[2] = z;
      layer.Last[2] = z;
      GatherBlock(camera, edges, light, layer, position, radius);
    }
    return;
  }
  // Parent boxes enclose the exact leaf boxes, including the light-far clamp.
  constexpr int block = 8;
  for (int z = bounds.First[2]; z <= bounds.Last[2]; ++z) {
    for (int by = bounds.First[1]; by <= bounds.Last[1]; by += block) {
      for (int bx = bounds.First[0]; bx <= bounds.Last[0]; bx += block) {
        const int ex = std::min(bx + block - 1, bounds.Last[0]);
        const int ey = std::min(by + block - 1, bounds.Last[1]);
        const Bounds cells{.First = {bx, by, z}, .Last = {ex, ey, z}, .Empty = false};
        CountWork(BlockTests);
        if (!Intersects(camera, edges, position, radius, cells)) { continue; }
        GatherBlock(camera, edges, light, cells, position, radius);
      }
    }
  }
}

void Builder::Gather(std::span<const Light> lights,
                     const Camera &camera,
                     std::span<const double> edges,
                     size_t indexBytes,
                     Method method) {
  for (uint32_t light = 0; light < lights.size(); ++light) {
    if (lights[light][3] == 0 || lights[light][7] == 0) {
      RetainGrowth(Globals, Globals.size() + 1);
      Globals.push_back(light);
      continue;
    }
    const double radius = 1.0 / lights[light][7];
    const Vector position = camera.View(lights[light]);
    GatherLight(camera, edges, position, radius, method, light);
    if (Pairs.size() > indexBytes / sizeof(uint32_t)) {
      Overflow = true;
      break;
    }
  }
  Assignments = Pairs.size();
  if (!Overflow) { return; }
  Pairs.clear();
  std::ranges::fill(Counts, 0);
  RetainGrowth(Globals, lights.size());
  Globals.resize(lights.size());
  std::ranges::iota(Globals, uint32_t{0});
}

void Builder::Pack(std::span<const double> edges, bool sort) {
  const auto start = static_cast<uint32_t>(4 + Counts.size() * 2);
  const auto global = start + static_cast<uint32_t>(Pairs.size());
  const auto edge = global + static_cast<uint32_t>(Globals.size());
  RetainGrowth(Words, edge + edges.size());
  Words.resize(edge + edges.size());
  Words[0] = global;
  Words[1] = static_cast<uint32_t>(Globals.size());
  Words[2] = 4;
  Words[3] = edge;
  uint32_t offset = start;
  RetainGrowth(Cursor, Counts.size());
  Cursor.resize(Counts.size());
  for (size_t cell = 0; cell < Counts.size(); ++cell) {
    Words[4 + cell * 2] = offset;
    Words[5 + cell * 2] = Counts[cell];
    Cursor[cell] = offset;
    offset += Counts[cell];
  }
  if (sort) {
    std::ranges::sort(Pairs);
    for (size_t i = 0; i < Pairs.size(); ++i) { Words[start + i] = Pairs[i].second; }
  } else {
    for (const auto &[cell, light] : Pairs) { Words[Cursor[cell]++] = light; }
  }
  std::ranges::copy(Globals, Words.begin() + global);
  for (size_t i = 0; i < edges.size(); ++i) {
    Words[edge + i] = std::bit_cast<uint32_t>(static_cast<float>(edges[i]));
  }
}

void Builder::Build(std::span<const Light> lights,
                    const Camera &camera,
                    std::span<const double> edges,
                    size_t indexBytes,
                    Method method) {
  Heap.Reset();
  Pairs.clear();
  Globals.clear();
  Tested = 0;
  BlockTests = 0;
  GrowthRetainedBytes = 0;
  Assignments = 0;
  Overflow = false;
  const uint32_t nx = (camera.Grid[0] + camera.Grid[3] - 1) / camera.Grid[3];
  const uint32_t ny = (camera.Grid[1] + camera.Grid[3] - 1) / camera.Grid[3];
  Counts.assign(static_cast<size_t>(nx) * ny * (edges.size() - 1), 0);
  Gather(lights, camera, edges, indexBytes, method);
  Pack(edges, method == Method::Sort);
}
}
