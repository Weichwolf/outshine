#include "BuildingFieldStreamingState.h"
#include "BuildingSourceRead.h"
#include "SurfacePreparation.h"
#include "StructureBuildQueue.h"
#include "SceneRenderer.h"
#include "Lens.h"
#include "MaterialFieldRasterInput.h"
#include "BuildingMaterials.h"
#include "PieceMesh.h"
#include "Tasks.h"
#include "TileGeodesy.h"
#include <algorithm>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <utility>

namespace outshine {
BuildingFieldStreaming::BuildingFieldStreaming() : State_(std::make_unique<State>()) {}

BuildingFieldStreaming::~BuildingFieldStreaming() = default;

BuildingFieldStreaming::Counts BuildingFieldStreaming::Read() const noexcept {
  return State_->Statistics;
}

Render::CameraBasis BuildingFieldStreaming::State::WorldCamera(const Context &context) {
  const auto direction = [&](const Vec3 &v) {
    return context.Frame.ToEcefDirection({.EastM = v[0], .NorthM = -v[2], .UpM = v[1]});
  };
  return {.EyeM = context.Frame.ToEcefPosition({.EastM = context.Camera.EyeM[0],
                                                .NorthM = -context.Camera.EyeM[2],
                                                .UpM = context.Camera.EyeM[1]}),
          .Forward = direction(context.Camera.Forward),
          .Right = direction(context.Camera.Right),
          .Up = direction(context.Camera.Up)};
}

void BuildingFieldStreaming::State::Discover(Context &context) {
  (void)Sources.Advance(context.Sources, context.Surface, 1);
  PollMetadata();
  const auto current = Sources.Entries();
  std::erase_if(Tiles, [&](const auto &tile) {
    const bool stale =
        tile.Index >= current.size() ||
        (current[tile.Index].Value && current[tile.Index].Value->BaseKey != tile.Source.BaseKey);
    if (!stale) { return false; }
    std::string error;
    (void)context.Pages.ActivateCells(tile.Index,
                                      {.Key = tile.SourceKey, .Occupied = tile.Source.Cells},
                                      tile.Originals,
                                      tile.Originals,
                                      error);
    context.Pages.Retire(tile.Index, tile.SourceKey, LevelOfDetail::Skyline);
    return true;
  });
  StartMetadata(context);
}

namespace {
std::optional<BuildingFieldRequest::Demand>
DemandFor(const BuildingFieldStreaming::Context &context,
          const BuildingSourceDemand::Source &source,
          const Box &bounds,
          const Render::CameraBasis &camera) {
  const double radius = Length(bounds.Span());
  const double distance = Length(camera.EyeM - bounds.Middle()) - radius;
  const auto projection = context.Surface.Footprints().Projection();
  if (distance <= 0 || Dot(bounds.Middle() - camera.EyeM, camera.Forward) <= radius ||
      projection.AllowedErrorPx <= 0 ||
      projection.FocalPx * radius * radius / (distance * distance) > projection.AllowedErrorPx) {
    return std::nullopt;
  }
  const double diameter = 2 * projection.FocalPx * radius / distance;
  const double required = std::ceil(diameter * 2) + 2;
  if (!std::isfinite(required) || required > 4096) { return std::nullopt; }
  const uint32_t pixels = std::bit_ceil(std::max(uint32_t{4}, static_cast<uint32_t>(required)));
  const auto geo = Ground::EcefToGeoWgs84(
      {.X = source.AnchorEcef[0], .Y = source.AnchorEcef[1], .Z = source.AnchorEcef[2]});
  const auto axes = EnuAxesEcef({.LongitudeDeg = geo.LongitudeDeg, .LatitudeDeg = geo.LatitudeDeg});
  const Vec3 delta = camera.EyeM - bounds.Middle();
  Vec3 direction{{Dot(delta, axes.East), Dot(delta, axes.Up), -Dot(delta, axes.North)}};
  if (!Normalise(direction)) { return std::nullopt; }
  const auto lens =
      Render::Lens::From(context.Camera, context.Renderer.PictureW(), context.Renderer.PictureH());
  if (!lens) { return std::nullopt; }
  return BuildingFieldRequest::Demand{.Directions = {direction},
                                      .Pixels = pixels,
                                      .View = {0},
                                      .GeometricView = BuildingFieldRequest::Pose{
                                          .Camera = camera,
                                          .Optics = *lens,
                                          .GeometryErrorMostPx = projection.AllowedErrorPx,
                                          .Space = BuildingFieldRequest::CoordinateSpace::World}};
}

std::optional<BuildingFieldRequest::Budget> BudgetFor(
    const Generators::PreparedBuildingAssets::SourceCell &cell, uint32_t pixels, uint32_t sources) {
  const auto field =
      ImpostorAtlas::EncodedBytesMost({.Pixels = static_cast<int>(pixels), .Views = 1});
  const size_t raster =
      (size_t{pixels} * pixels * 2 + Generators::BuildingSurfaceMaterials().size()) *
      sizeof(Render::MaterialFieldRasterInput::Word);
  constexpr size_t most = std::numeric_limits<size_t>::max();
  if (!field || cell.HeaderBytes > most / 3 || cell.NativeBytes > most / 16 ||
      cell.HeaderBytes * 3 > most - cell.NativeBytes * 16 || *field > (most - raster) / 2 ||
      cell.HeaderBytes * 3 + cell.NativeBytes * 16 > most - *field * 2 - raster) {
    return std::nullopt;
  }
  const size_t producer = cell.HeaderBytes * 3 + cell.NativeBytes * 16 + *field * 2 + raster;
  return BuildingFieldRequest::Budget{
      .ProducerBytes = producer,
      .ResidentBytes = *field,
      .FieldBytesMost = *field,
      .RasterBytesMost = raster,
      .Capture = {.Samples = uint64_t{pixels} * pixels,
                  .Sources = sources,
                  .Faces = cell.NativeBytes / sizeof(uint32_t),
                  .TriangleAttempts =
                      producer / (3 * sizeof(StoredVertex) + 3 * sizeof(uint32_t))}};
}
}

bool BuildingFieldStreaming::State::RequestGroup(Context &context,
                                                 Tile &tile,
                                                 Cell &cell,
                                                 Group &group) {
  if (group.Where != Phase::Waiting) { return false; }
  auto demand = DemandFor(context, tile.Source, group.Bounds, WorldCamera(context));
  if (!demand) { return false; }
  auto count = tile.Source.CellSourceCounts[cell.Source.Cell - 1u];
  if (!group.Sources.empty()) {
    demand->Sources = group.Sources;
    count = static_cast<uint32_t>(group.Sources.size());
  }
  const auto budget = BudgetFor(cell.Source, demand->Pixels, count);
  if (!budget) {
    Reject(cell);
    return false;
  }
  auto source = tile.Source;
  source.Cells = uint64_t{1} << (cell.Source.Cell - 1u);
  source.Bounds = group.Bounds;
  if (!SourceRead || SourceRead->Key() != source.BaseKey || ReadCell != cell.Source.Cell) {
    SourceRead =
        std::make_shared<BuildingSourceRead>(context.Surface.BuildingAssets(), source.BaseKey);
    ReadCell = cell.Source.Cell;
  }
  auto request = BuildingFieldRequest::Start(context.Pipeline,
                                             context.Workers,
                                             context.Surface.AssetStore(),
                                             context.Surface.BuildingAssets(),
                                             std::move(source),
                                             *demand,
                                             *budget,
                                             SourceRead);
  if (!request) { return false; }
  group.Request.emplace(std::move(*request));
  group.Where = Phase::Pending;
  cell.Where = Phase::Pending;
  return true;
}

void BuildingFieldStreaming::State::RequestOne(Context &context) {
  for (const auto &tile : Tiles) {
    for (const auto &cell : tile.Cells) {
      if (std::ranges::any_of(cell.Groups,
                              [](const Group &g) { return g.Where == Phase::Pending; })) {
        return;
      }
    }
  }
  for (auto &tile : Tiles) {
    for (auto &cell : tile.Cells) {
      if (cell.Where == Phase::Rejected || cell.Where == Phase::Ready) { continue; }
      for (auto &group : cell.Groups) {
        if (RequestGroup(context, tile, cell, group)) { return; }
        if (cell.Where == Phase::Rejected) { break; }
      }
    }
  }
  SourceRead.reset();
}

bool BuildingFieldStreaming::Advance(Context &context, std::string &error) {
  if (!context.Surface.BuildingAssets() || !context.Surface.AssetStore()) { return true; }
  State_->Discover(context);
  State_->PollFields(context);
  State_->RequestOne(context);
  if (!State_->Select(context, error)) { return false; }
  auto &counts = State_->Statistics;
  counts.Sources = State_->Tiles.size();
  counts.Pending = State_->Reading ? 1 : 0;
  for (const auto &entry : State_->Sources.Entries()) {
    counts.Pending += entry.Where == BuildingSourceDemand::Phase::Pending ? 1 : 0;
  }
  counts.Ready = counts.Rejected = counts.ReadyGroups = 0;
  for (const auto &tile : State_->Tiles) {
    for (const auto &cell : tile.Cells) {
      counts.Pending += static_cast<size_t>(std::ranges::count_if(
          cell.Groups, [](const State::Group &g) { return g.Where == State::Phase::Pending; }));
      counts.ReadyGroups += static_cast<size_t>(std::ranges::count_if(
          cell.Groups, [](const State::Group &g) { return g.Where == State::Phase::Ready; }));
      counts.Ready += cell.Where == State::Phase::Ready ? 1 : 0;
      counts.Rejected += cell.Where == State::Phase::Rejected ? 1 : 0;
    }
  }
  return true;
}
}
