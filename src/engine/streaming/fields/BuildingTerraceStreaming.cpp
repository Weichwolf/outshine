#include "BuildingFieldStreamingState.h"
#include "BuildingMaterials.h"
#include "BuildingSourceRead.h"
#include "GeometryPageView.h"
#include "SceneRenderer.h"
#include "StructureBuildQueue.h"
#include "SurfacePreparation.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <ranges>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

namespace outshine {
namespace {
constexpr unsigned kSpacingM = 8;
constexpr unsigned kSupportMostM = 16;
constexpr size_t kProductBytesMost = size_t{64} * 1024u;
constexpr size_t kProductSamplesMost = size_t{256} * 1024u;
constexpr size_t kNativeBytesLeast = 4096;
}

bool BuildingFieldStreaming::State::AllowsTerrace(const Context &context,
                                                  const Tile &tile,
                                                  const Cell &cell,
                                                  const Vec3 &eye) {
  if (cell.Source.NativeBytes < kNativeBytesLeast || cell.Source.Bounds.Empty() ||
      tile.Source.CellSourceCounts[cell.Source.Cell - 1] < 2) {
    return false;
  }
  const auto projection = context.Surface.Footprints().Projection();
  const Vec2 viewport{{context.Renderer.PictureW(), context.Renderer.PictureH()}};
  constexpr double silhouetteSupportPx = 3.0;
  const double support = silhouetteSupportPx * std::min(1.0, projection.AllowedErrorPx);
  ViewPath path{.Start = eye, .End = eye};
  if (context.PlannedView &&
      context.PlannedView->DistanceTo(context.Camera.EyeM) <= context.PlannedView->RadiusM) {
    path = ViewPathInEcef(*context.PlannedView, context.Frame);
  }
  return SourceProductSelection::ProjectedSupportLimitM(
             cell.Source.Bounds, path, viewport, projection.FocalPx, support) >= kSupportMostM;
}

bool BuildingFieldStreaming::State::StageTerrace(
    Context &context,
    const Tile &tile,
    Cell &cell,
    const Generators::Osm::UrbanPrismPreparation::Product &ready,
    const std::string &key) {
  if (!TerraceMaterial) {
    Geometry geometry;
    Material wall;
    for (size_t axis = 0; axis < 3; ++axis) {
      wall.BaseColour[axis] = Generators::kBuildingWallColour[axis];
    }
    constexpr float roughness = .8f;
    wall.Roughness = roughness;
    if (!geometry.addSurface("building terraces", wall)) {
      cell.TerraceRejected = true;
      return true;
    }
    const auto registered = context.Renderer.RegisterPieceMaterials(std::move(geometry));
    if (!registered) {
      cell.TerraceRejected = true;
      return true;
    }
    TerraceMaterial = *registered;
  }
  const auto roof = Generators::BuildingRoofCoating(.5f);
  std::vector<Render::PrismInstance> rows;
  rows.reserve(ready->Placements.size());
  for (uint32_t patch = 0; patch < ready->Placements.size(); ++patch) {
    const auto scene = ToScene(context.Frame, ready->Placements[patch].EcefFromLocal);
    Mat4f row;
    std::ranges::transform(
        scene, row.begin(), [](double value) { return static_cast<float>(value); });
    rows.push_back({.Patch = patch, .Current = row, .Previous = row, .TopCoating = roof});
  }
  const Render::GeometryPageView page{
      .Parts = {},
      .AssetKey = key,
      .SelectedView = {},
      .RequestedDetail = LevelOfDetail::Skyline,
      .RequestedCell = cell.Source.Cell,
      .OccupiedCells = uint64_t{1} << (cell.Source.Cell - 1),
      .Coverage = cell.Coverage,
      .Grid = Render::GeometryPageGridView{
          .Grid = std::shared_ptr<const PrismGrid>(ready, &ready->Geometry),
          .SceneInstances = rows,
          .Material = *TerraceMaterial,
          .Passes = Render::GeometryPasses::Main}};
  const auto staged =
      context.Pages.StageProduct(tile.Index, page, tile.Source.AnchorEcef, tile.SourceKey);
  if (!staged && staged.error().Kind == Render::Native::ErrorKind::Busy) { return false; }
  if (staged) {
    cell.TerraceKey = key;
  } else {
    cell.TerraceRejected = true;
  }
  return true;
}

void BuildingFieldStreaming::State::AdvanceTerrace(Context &context) {
  const auto eye = WorldCamera(context).EyeM;
  if (Terrace) {
    PollTerrace(context, eye, *Terrace);
  } else {
    RequestTerrace(context, eye);
  }
}

void BuildingFieldStreaming::State::PollTerrace(Context &context,
                                                const Vec3 &eye,
                                                TerraceWork &work) {
  if (work.Retiring) {
    if (!work.Job->InFlight()) { Terrace.reset(); }
    return;
  }
  const auto tile = std::ranges::find(Tiles, work.Tile, &Tile::Index);
  if (tile == Tiles.end() || work.Revision != tile->SourceKey ||
      StructureBuildQueue::QualifiedSourceKey(context.Surface.Footprints(), tile->Index) !=
          work.Revision) {
    work.Job->Cancel();
    work.Retiring = true;
    return;
  }
  const auto cell = std::ranges::find(
      tile->Cells, work.Cell, [](const Cell &candidate) { return candidate.Source.Cell; });
  if (cell == tile->Cells.end() || !AllowsTerrace(context, *tile, *cell, eye)) {
    work.Job->Cancel();
    work.Retiring = true;
    return;
  }
  const auto ready = work.Job->Advance([&](const std::stop_token &stop) {
    return AssetPackageFactory{[source = TerraceSourceRead, request = work.Request, stop](
                                   size_t bytesMost) -> std::expected<AssetPackage, std::string> {
      if (stop.stop_requested()) { return std::unexpected("terrace generation cancelled"); }
      const auto base = source->Load();
      if (!base || !*base) { return std::unexpected("terrace source is unavailable"); }
      return PackagePreparedTerraces(
          **base,
          request,
          {.SamplesMost = kProductSamplesMost, .PointTestsMost = 4 * kProductSamplesMost},
          bytesMost,
          stop);
    }};
  });
  if (!ready) {
    cell->TerraceRejected = true;
    Terrace.reset();
  } else if (*ready && StageTerrace(context, *tile, *cell, *ready, work.Job->Key())) {
    Terrace.reset();
  }
}

void BuildingFieldStreaming::State::RequestTerrace(Context &context, const Vec3 &eye) {
  using namespace Generators::Osm;
  for (auto &tile : Tiles) {
    if (!tile.Coverage || StructureBuildQueue::QualifiedSourceKey(context.Surface.Footprints(),
                                                                  tile.Index) != tile.SourceKey) {
      continue;
    }
    for (auto &cell : tile.Cells) {
      if (cell.TerraceRejected || !cell.TerraceKey.empty() ||
          !AllowsTerrace(context, tile, cell, eye)) {
        continue;
      }
      const TerraceAssetRequest request{.BaseKey = tile.Source.BaseKey,
                                        .SpacingM = kSpacingM,
                                        .Cells = uint64_t{1} << (cell.Source.Cell - 1),
                                        .SupportMostM = kSupportMostM};
      const auto key = request.Key();
      if (!key) {
        cell.TerraceRejected = true;
        continue;
      }
      const size_t bytes = std::min(kProductBytesMost, cell.Source.NativeBytes / 2);
      if (!TerraceSourceRead || TerraceSourceRead->Key() != tile.Source.BaseKey) {
        TerraceSourceRead = std::make_shared<BuildingSourceRead>(context.Surface.BuildingAssets(),
                                                                 tile.Source.BaseKey);
      }
      Terrace = TerraceWork{.Tile = tile.Index,
                            .Revision = tile.SourceKey,
                            .Cell = cell.Source.Cell,
                            .Request = request,
                            .Job = std::make_unique<UrbanPrismPreparation>(
                                context.Workers, context.Surface.AssetStore(), *key, bytes)};
      return;
    }
  }
  TerraceSourceRead.reset();
}
}
