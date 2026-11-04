#include "BuildingFieldStreamingState.h"
#include "ImpostorSurface.h"
#include "SceneRenderer.h"
#include "SurfacePreparation.h"
#include "StructureBuildQueue.h"
#include "Lens.h"
#include "PieceMesh.h"
#include "Log.h"
#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace outshine {
void BuildingFieldStreaming::State::PollFields(Context &context) {
  for (auto &tile : Tiles) {
    for (auto &cell : tile.Cells) {
      if (cell.Where != Cell::Phase::Pending || !cell.Request) { continue; }
      const auto ready = cell.Request->Poll();
      if (!ready) {
        if (LastRejection != ready.error()) {
          Log::Warn(LogTag::World, "building_field_rejected", {{"reason", ready.error()}});
        }
        LastRejection = ready.error();
        cell.Request.reset();
        cell.Where = Cell::Phase::Rejected;
        continue;
      }
      if (!*ready) { continue; }
      cell.Product = *ready;
      std::string error;
      if (!Stage(context, tile, cell, error)) {
        if (LastRejection != error) {
          Log::Warn(LogTag::World, "building_field_staging_rejected", {{"reason", error}});
        }
        LastRejection = std::move(error);
        cell.Product.reset();
        cell.Request.reset();
        cell.Where = Cell::Phase::Rejected;
        continue;
      }
      cell.Metadata = cell.Product->Metadata->Metadata();
      cell.Support = cell.Product->GeometricSupport;
      cell.Product.reset();
      cell.Request.reset();
      cell.Where = Cell::Phase::Ready;
    }
  }
}

bool BuildingFieldStreaming::State::Stage(Context &context,
                                          Tile &tile,
                                          Cell &cell,
                                          std::string &error) {
  const auto &product = *cell.Product;
  if (!product.Field || !product.Metadata || !product.GeometricSupport ||
      !product.Metadata->Metadata().WorldFromField) {
    error = "building field has no complete registered source or geometric support";
    return false;
  }
  auto geometry =
      Render::BuildImpostorSurface(*product.Field, 0, Render::ImpostorSurfaceDetail::Flat);
  if (!geometry || geometry->parts() != 1) {
    error = "building field has no opaque PBR surface";
    return false;
  }
  const Mat4 &world = *product.Metadata->Metadata().WorldFromField;
  const auto positions = geometry->positionsOf(0);
  const auto normals = geometry->normalsOf(0);
  const auto texture = geometry->textureOf(0);
  const auto sourceTangents = geometry->tangentsOf(0);
  std::vector<StoredVertex> vertices;
  std::vector<float> tangents;
  vertices.reserve(positions.size() / 3);
  tangents.reserve(sourceTangents.size());
  const auto rotate = [&](const Vec3 &v) {
    return world.TransformPoint(v) - world.TransformPoint(Vec3{});
  };
  for (size_t at = 0; at < positions.size() / 3; ++at) {
    const Vec3 p =
        world.TransformPoint({{positions[at * 3], positions[at * 3 + 1], positions[at * 3 + 2]}}) -
        tile.Source.AnchorEcef;
    const Vec3 n = rotate({{normals[at * 3], normals[at * 3 + 1], normals[at * 3 + 2]}});
    vertices.push_back(StoredVertex::Of(
        {{static_cast<float>(p[0]), static_cast<float>(p[1]), static_cast<float>(p[2])}},
        {{texture[at * 2], texture[at * 2 + 1]}},
        {{static_cast<float>(n[0]), static_cast<float>(n[1]), static_cast<float>(n[2])}}));
    if (!sourceTangents.empty()) {
      const Vec3 t = rotate(
          {{sourceTangents[at * 4], sourceTangents[at * 4 + 1], sourceTangents[at * 4 + 2]}});
      tangents.insert(tangents.end(),
                      {static_cast<float>(t[0]),
                       static_cast<float>(t[1]),
                       static_cast<float>(t[2]),
                       sourceTangents[at * 4 + 3]});
    }
  }
  const auto indices = geometry->trianglesOf(0);
  const std::vector<uint32_t> triangles(indices.begin(), indices.end());
  auto material = context.Renderer.RegisterPieceMaterials(std::move(*geometry));
  if (!material) {
    error = material.error();
    return false;
  }
  const Render::GeometryPagePartView part{.Tangents = tangents,
                                          .Verts = vertices,
                                          .Indices = triangles,
                                          .Clusters = {},
                                          .Colours = {},
                                          .Uv1 = {},
                                          .Surface = Render::PieceSurface::Registered(*material),
                                          .Textured = true,
                                          .Passes = Render::GeometryPasses::Main};
  cell.AssetKey = product.Metadata->FieldKey({0});
  uint64_t digest = 0;
  (void)std::from_chars(cell.AssetKey.data(), cell.AssetKey.data() + 16, digest, 16);
  const Render::GeometryPageView page{.Parts = std::span(&part, 1),
                                      .Digest = digest,
                                      .AssetKey = cell.AssetKey,
                                      .FallbackHeights = false,
                                      .SelectedView = std::nullopt,
                                      .RequestedDetail = LevelOfDetail::Skyline,
                                      .RequestedCell = cell.Source.Cell,
                                      .OccupiedCells = uint64_t{1} << (cell.Source.Cell - 1u)};
  cell.Staged = context.Pages.StageCell(
      tile.Index, cell.Source.Cell, page, tile.Source.AnchorEcef, error, tile.SourceKey);
  return cell.Staged;
}

bool BuildingFieldStreaming::State::ValidPose(const Context &context, const Cell &cell) {
  if (!cell.Staged || !cell.Support || !cell.Metadata || !cell.Metadata->WorldFromField) {
    return false;
  }
  const auto lens =
      Render::Lens::From(context.Camera, context.Renderer.PictureW(), context.Renderer.PictureH());
  const auto hull = Render::ImpostorPoseBounds::From(cell.Support->Plane);
  if (!lens || !hull) { return false; }
  const auto camera = WorldCamera(context);
  const double radius = Length(cell.Source.Bounds.Span());
  const double distance = Length(camera.EyeM - cell.Source.Bounds.Middle()) - radius;
  const auto projection = context.Surface.Footprints().Projection();
  if (distance <= 0 || 4 * projection.FocalPx * radius / distance + 2 > cell.Metadata->Pixels) {
    return false;
  }
  const auto bound = hull->Project(camera, *lens, *cell.Metadata->WorldFromField);
  return bound && bound->DepthInside && bound->L1UpperPx <= projection.AllowedErrorPx;
}

bool BuildingFieldStreaming::State::Select(Context &context, std::string &error) {
  Statistics.Active = 0;
  for (auto &tile : Tiles) {
    const auto qualified =
        StructureBuildQueue::QualifiedSourceKey(context.Surface.Footprints(), tile.Index);
    if (!qualified) { continue; }
    const uint64_t key = *qualified;
    std::vector<Render::GeometryPageResidency::CellSelection> original;
    context.Pages.ForEachDigest(
        [&](const auto &page) {
          if (page.Tile == tile.Index && page.SourceKey == key && page.Cell != 0 &&
              page.Detail != LevelOfDetail::Skyline) {
            original.push_back(
                {.Cell = page.Cell, .Detail = page.Detail, .OccupiedCells = page.OccupiedCells});
          }
        },
        Render::GeometryPasses::Shadow);
    if (original.empty()) { continue; }
    auto main = original;
    const bool parentsValid =
        context.Pages.HasProjectedTile(tile.Index,
                                       {.Key = key, .Occupied = tile.Source.Cells},
                                       context.Eye,
                                       context.Surface.Footprints().Projection(),
                                       context.Surface.Footprints().EyeEcef(),
                                       Render::GeometryPasses::Shadow);
    uint64_t coverage = 0;
    uint64_t active = 0;
    for (auto &choice : main) {
      coverage |= choice.Mask();
      const auto found =
          std::ranges::find(tile.Cells, choice.Cell, [](const Cell &c) { return c.Source.Cell; });
      if (parentsValid && key == tile.SourceKey && choice.Cell > 0 && choice.Cell <= 64 &&
          found != tile.Cells.end() && found->Where == Cell::Phase::Ready &&
          choice.Mask() == (uint64_t{1} << (choice.Cell - 1u)) && ValidPose(context, *found)) {
        choice.Detail = LevelOfDetail::Skyline;
        active |= choice.Mask();
      }
    }
    Statistics.Active += static_cast<size_t>(std::popcount(active));
    if (active == tile.Active && key == tile.SelectedKey && coverage == tile.SelectedCoverage) {
      continue;
    }
    if (!context.Pages.ActivateCells(
            tile.Index, {.Key = key, .Occupied = coverage}, main, original, error)) {
      return false;
    }
    tile.Active = active;
    tile.SelectedKey = key;
    tile.SelectedCoverage = coverage;
  }
  return true;
}
}
