#include "EngineState.h"
#include "Box.h"
#include "GroundPatchwork.h"
#include "RuntimeScene.h"
#include "TangentFrame.h"
#include "TerrainClipmapPreparation.h"
#include "TerrainFieldSet.h"
#include "TerrainLoader.h"
#include "WorldRegionBuildState.h"
#include "Units.h"
#include "device/native/MemoryStatus.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace outshine {
namespace {
constexpr size_t kClipmapSamplesPerSlice = 1024;
constexpr Render::ClipmapGrid kFinestGrid{.HalfCells = 128, .Levels = 1};
constexpr double kFinestSampleHalfWidthM = (kFinestGrid.HalfCells + 3) * kFinestGrid.CellM;

Vec2 GroundCamera(const Core::RuntimeScene &scene) {
  const auto &eye = scene.Watched() ? scene.Watching() : scene.Aimed();
  return {{eye.EyeM[0], -eye.EyeM[2]}};
}

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
  Render::ClipmapGrid grid = kFinestGrid;
  grid.ViewRadiusM = sightM;
  double reach = (grid.HalfCells - 2) * grid.CellM;
  const double required = sightM + reach;
  while (reach < required && std::isfinite(reach)) {
    ++grid.Levels;
    reach *= 2;
  }
  return grid;
}

Box ClipmapSampleVolume(Vec2 camera, const std::array<double, 2> &heightsM) noexcept {
  constexpr double radiusM = kWgs84A * (1 - kWgs84E2);
  constexpr double roundoffM = 256 * std::numeric_limits<double>::epsilon() * kWgs84A;
  const double heightM = std::max(std::abs(heightsM[0]), std::abs(heightsM[1]));
  const double reachM = 2 * (std::hypot(std::abs(camera[0]) + kFinestSampleHalfWidthM,
                                        std::abs(camera[1]) + kFinestSampleHalfWidthM) +
                             roundoffM);
  if (!(reachM < radiusM / 2) || !(heightM < radiusM / 4)) { return {}; }
  const double sagM = (reachM * reachM) / radiusM;
  const double driftM = heightM * std::hypot(reachM, sagM) / radiusM;
  const double half = kFinestSampleHalfWidthM + driftM + roundoffM;
  return {.Min = {{camera[0] - half, camera[1] - half, -sagM - heightM - roundoffM}},
          .Max = {{camera[0] + half, camera[1] + half, heightM + roundoffM}}};
}
}

double Engine::State::FinestTerrainSampleRadiusM() noexcept {
  return std::numbers::sqrt2 * FinestTerrainSampleHalfWidthM();
}

double Engine::State::FinestTerrainSampleHalfWidthM() noexcept {
  return kFinestSampleHalfWidthM;
}

bool Engine::State::PublishedGroundCoversClipmap(const Around &request) const {
  if (!World.PublishedGroundCoverage) { return false; }
  const auto &coverage = *World.PublishedGroundCoverage;
  if (PatchworkCoversFocus(coverage, request, FinestTerrainSampleRadiusM())) { return true; }
  const auto *terrain = World.TerrainSurface.PublishedSurface();
  if (terrain == nullptr) { return false; }
  const auto range = terrain->SampleAslBoundsM();
  if (!range) { return false; }
  const auto bounds = ClipmapSampleVolume(GroundCamera(*Picture.Standing), *range);
  const auto &anchor = Session.Declared.Ground.Origin;
  const auto frame =
      TangentFrame::At({.LongitudeDeg = anchor.LongitudeDeg, .LatitudeDeg = anchor.LatitudeDeg});
  return PatchworkCoversSamples(coverage, request, frame, bounds);
}

bool Engine::State::TerrainClipmapsReady() const noexcept {
  return World.TerrainSurface.PublishedSurface() == nullptr ||
         (World.TerrainClipmaps && Picture.Standing &&
          World.TerrainClipmaps->ReadyAt(GroundCamera(*Picture.Standing)));
}

Result Engine::State::AdvanceTerrainClipmaps() {
  if (!World.TerrainClipmaps || !Picture.Standing) { return {}; }
  const size_t bytesMost = AvailableFieldBytes(Picture.Device.ReadMemoryStatus());
  const Vec2 camera = GroundCamera(*Picture.Standing);
  const auto advanced =
      World.TerrainClipmaps->Advance(camera,
                                     {.WorkBytesMost = bytesMost,
                                      .StagingBytesMost = bytesMost,
                                      .SamplesPerSlice = kClipmapSamplesPerSlice});
  Published.RecordMetric(
      "terrain clipmaps: prepared", World.TerrainClipmaps->ReadyAt(camera) ? 1 : 0, "bool");
  Published.RecordMetric("terrain clipmaps: worker storage",
                         static_cast<double>(World.TerrainClipmaps->HeapBytes()),
                         "bytes");
  return advanced;
}

bool Engine::State::CandidateTerrainClipmapsReady(
    const WorldRegionBuildState &state) const noexcept {
  const auto *clipmaps = state.StagedClipmaps();
  return state.Candidate().Products().TerrainSurface.PublishedSurface() == nullptr ||
         (clipmaps != nullptr && Picture.Standing &&
          clipmaps->ReadyAt(GroundCamera(*Picture.Standing)));
}

Result Engine::State::AdvanceCandidateTerrainClipmaps(WorldRegionBuildState &state) {
  if (!state.Residency().ResidencyReady || !Picture.Standing ||
      CandidateTerrainClipmapsReady(state)) {
    return {};
  }
  if (!Services.Compute) { return std::unexpected("terrain clipmaps require a compute worker"); }
  const size_t bytesMost = AvailableFieldBytes(Picture.Device.ReadMemoryStatus());
  auto &clipmaps = state.StagedClipmaps();
  if (!clipmaps) {
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
    const auto &terrain = state.Candidate().Products().TerrainSurface;
    auto source = std::make_shared<TerrainSurfacePublication>(terrain);
    source->SetRenderFallback(
        std::make_shared<const Generators::TerrainFieldSet>(std::move(parents)));
    auto surface = Picture.Device.CreateTerrainClipmaps(GridFor(TerrainSightM()), bytesMost);
    if (!surface) {
      if (surface.error().Kind == Render::Native::ErrorKind::Busy) { return {}; }
      return std::unexpected(surface.error().Message);
    }
    clipmaps = std::make_unique<TerrainClipmapPreparation>(
        *Services.Compute, std::move(source), std::move(*surface));
  }
  return clipmaps->Advance(GroundCamera(*Picture.Standing),
                           {.WorkBytesMost = bytesMost,
                            .StagingBytesMost = bytesMost,
                            .SamplesPerSlice = kClipmapSamplesPerSlice});
}
}
