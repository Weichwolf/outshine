#include "EngineState.h"
#include "TerrainClipmapPreparation.h"
#include "TerrainFieldSet.h"
#include "TerrainLoader.h"
#include "device/native/MemoryStatus.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace outshine {
namespace {
size_t AvailableFieldBytes(const Render::Native::MemoryStatus &status) {
  uint64_t bytes = std::numeric_limits<size_t>::max();
  if (status.RecommendedWorkingSetBytes && status.ResourceAllocatedBytes) {
    bytes = *status.RecommendedWorkingSetBytes -
            std::min(*status.RecommendedWorkingSetBytes, *status.ResourceAllocatedBytes);
  }
  for (const auto &heap : status.Heaps) {
    if (heap.DeviceLocal && heap.EstimatedBudgetBytes && heap.EstimatedUsageBytes) {
      bytes = std::min(bytes,
                       *heap.EstimatedBudgetBytes -
                           std::min(*heap.EstimatedBudgetBytes, *heap.EstimatedUsageBytes));
    }
  }
  return static_cast<size_t>(bytes);
}

Render::ClipmapGrid GridFor(double sightM) {
  Render::ClipmapGrid grid{.HalfCells = 128, .Levels = 1};
  double reach = grid.HalfCells - 2;
  const double required = sightM + reach;
  while (reach < required && std::isfinite(reach)) {
    ++grid.Levels;
    reach *= 2;
  }
  return grid;
}
}

bool Engine::State::TerrainClipmapsReady() const noexcept {
  return World.TerrainSurface.PublishedSurface() == nullptr ||
         (World.ClipmapPublication == World.Relaid && World.TerrainClipmaps &&
          World.TerrainClipmaps->Ready());
}

Result Engine::State::AdvanceTerrainClipmaps() {
  if (World.TerrainSurface.PublishedSurface() == nullptr || !Services.Compute ||
      !Picture.Standing) {
    return {};
  }
  const size_t bytesMost = AvailableFieldBytes(Picture.Device.ReadMemoryStatus());
  if (World.ClipmapPublication != World.Relaid || !World.TerrainClipmaps) {
    std::shared_ptr<const Ground::TerrainField> parent;
    std::optional<Data::SourceFailure> failure;
    constexpr Data::TileId root{.Zoom = 0, .X = 0, .Y = 0};
    const auto status = World.Surface().Ground().PollStitchedField(root, parent, &failure);
    if (status == Ground::TilePool::Reply::Pending || status == Ground::TilePool::Reply::Deferred) {
      return {};
    }
    if (status != Ground::TilePool::Reply::Ready || !parent) {
      return std::unexpected(failure ? failure->Describe() : "clipmap DEM parent is unavailable");
    }
    std::vector<Generators::TerrainFieldSet::Entry> parents;
    parents.emplace_back(root, std::move(parent));
    auto source = std::make_shared<TerrainSurfacePublication>(World.TerrainSurface);
    source->SetRenderFallback(
        std::make_shared<const Generators::TerrainFieldSet>(std::move(parents)));
    auto surface = Picture.Device.CreateTerrainClipmaps(GridFor(TerrainSightM()), bytesMost);
    if (!surface) { return std::unexpected(surface.error().Message); }
    World.TerrainClipmaps = std::make_unique<TerrainClipmapPreparation>(
        *Services.Compute, std::move(source), std::move(*surface));
    World.ClipmapPublication = World.Relaid;
  }
  const auto advanced = World.TerrainClipmaps->Advance(
      Picture.Device.TerrainClipmapPosition(),
      {.WorkBytesMost = bytesMost, .StagingBytesMost = bytesMost, .SamplesPerSlice = 1024});
  Published.RecordMetric(
      "terrain clipmaps: prepared", World.TerrainClipmaps->Ready() ? 1 : 0, "bool");
  Published.RecordMetric("terrain clipmaps: worker storage",
                         static_cast<double>(World.TerrainClipmaps->HeapBytes()),
                         "bytes");
  return advanced;
}
}
