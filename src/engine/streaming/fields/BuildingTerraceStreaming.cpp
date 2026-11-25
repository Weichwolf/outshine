#include "BuildingFieldStreamingState.h"
#include "BuildingGeometrySource.h"
#include "BuildingMaterials.h"
#include "BuildingSourceRead.h"
#include "GeometryPageView.h"
#include "SceneRenderer.h"
#include "StructureBuildQueue.h"
#include "SurfacePreparation.h"
#include <algorithm>
#include <atomic>
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

std::expected<std::optional<GeometryAsset::Product>, std::string>
LoadDetail(AssetCache &cache,
           Generators::PreparedBuildingAssets &assets,
           const Generators::Osm::TerraceAssetRequest &request,
           const SourceCoverage &cell,
           size_t bytesMost,
           const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected("terrace detail cancelled"); }
  auto remainder = Generators::Osm::ReadPreparedTerraceRemainder(cache, request, kProductBytesMost);
  if (!remainder || remainder->SourceCount != cell.SourceCount()) {
    return std::unexpected("terrace detail has a foreign source domain");
  }
  if (remainder->Sources.empty()) { return std::nullopt; }
  const auto expected =
      SourceCoverage::FromSources(request.BaseKey, remainder->SourceCount, remainder->Sources);
  if (!expected || !cell.Contains(*expected) || *expected == cell) {
    return std::unexpected("terrace detail exceeds its selected source partition");
  }
  const auto recipe =
      Generators::BuildingGeometrySource::Recipe{.BaseKey = request.BaseKey,
                                                 .SourceCount = remainder->SourceCount,
                                                 .Sources = std::move(remainder->Sources),
                                                 .Detail = LevelOfDetail::Fine};
  auto result =
      Generators::BuildingGeometrySource::Resolve(cache,
                                                  &assets,
                                                  recipe,
                                                  {.EncodedBytesMost = bytesMost,
                                                   .TriangleAttempts = bytesMost / sizeof(uint32_t),
                                                   .KeepWhenCacheBusy = true},
                                                  stop);
  if (!result || result->Layout.Coverage != *expected) {
    return std::unexpected("native terrace detail does not cover the retained buildings");
  }
  return std::move(*result);
}
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
    const std::string &key,
    const std::optional<GeometryAsset::Product> &detail) {
  if (!TerraceMaterial) {
    const auto registered = RegisterPaint(context.Renderer, Generators::BuildingSurfaceMaterials());
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
  const auto remainder = detail ? MeshParts(*detail, *TerraceMaterial) : RemainderParts{};
  const auto detailPlacement =
      detail ? std::make_shared<const Mat4>(ToScene(context.Frame, detail->Layout.WorldFromLocal))
             : nullptr;
  const Render::GeometryPageView page{
      .Parts = remainder.Parts,
      .AssetKey = key,
      .SelectedView = {},
      .RequestedDetail = LevelOfDetail::Skyline,
      .RequestedCell = cell.Source.Cell,
      .OccupiedCells = uint64_t{1} << (cell.Source.Cell - 1),
      .Coverage = cell.Coverage,
      .Placement = detailPlacement,
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

bool BuildingFieldStreaming::State::PrepareTerraceDetail(Context &context,
                                                         const Cell &cell,
                                                         TerraceWork &work) {
  const auto load = std::make_shared<TerraceWork::DetailLoad>();
  if (!context.Workers.PostDetached([load,
                                     cache = context.Surface.AssetStore(),
                                     assets = context.Surface.BuildingAssets(),
                                     request = work.Request,
                                     coverage = cell.Coverage,
                                     bytes = cell.Source.NativeBytes / 2] {
        load->Result =
            LoadDetail(*cache, *assets, request, coverage, bytes, load->Stop.get_token());
        load->Done.store(true, std::memory_order_release);
      })) {
    return false;
  }
  work.Detail = load;
  return true;
}

bool BuildingFieldStreaming::State::PublishTerrace(
    Context &context,
    const Tile &tile,
    Cell &cell,
    TerraceWork &work,
    const Generators::Osm::UrbanPrismPreparation::Product &ready) {
  if (!work.Detail) {
    (void)PrepareTerraceDetail(context, cell, work);
    return false;
  }
  if (!work.Detail->Done.load(std::memory_order_acquire)) { return false; }
  const auto &detail = work.Detail->Result;
  if (!detail) {
    cell.TerraceRejected = true;
    return true;
  }
  return StageTerrace(context, tile, cell, ready, work.Job->Key(), *detail);
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
    if (!work.Job->InFlight() &&
        (!work.Detail || work.Detail->Done.load(std::memory_order_acquire))) {
      Terrace.reset();
    }
    return;
  }
  const auto tile = std::ranges::find(Tiles, work.Tile, &Tile::Index);
  if (tile == Tiles.end() || work.Revision != tile->SourceKey ||
      StructureBuildQueue::QualifiedSourceKey(context.Surface.Footprints(), tile->Index) !=
          work.Revision) {
    work.Cancel();
    return;
  }
  const auto cell = std::ranges::find(
      tile->Cells, work.Cell, [](const Cell &candidate) { return candidate.Source.Cell; });
  if (cell == tile->Cells.end() || !AllowsTerrace(context, *tile, *cell, eye)) {
    work.Cancel();
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
  } else if (*ready && PublishTerrace(context, *tile, *cell, work, *ready)) {
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
                                        .SupportMostM = kSupportMostM,
                                        .RetainRemainder = true};
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
                                context.Workers, context.Surface.AssetStore(), *key, bytes),
                            .Detail = {}};
      return;
    }
  }
  TerraceSourceRead.reset();
}
}
