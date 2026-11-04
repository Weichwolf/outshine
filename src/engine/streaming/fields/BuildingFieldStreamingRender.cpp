#include "BuildingFieldStreamingState.h"
#include "ImpostorSurface.h"
#include "SceneRenderer.h"
#include "SurfacePreparation.h"
#include "StructureBuildQueue.h"
#include "Lens.h"
#include "PieceMesh.h"
#include "Log.h"
#include "Sha256.h"
#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine {
void BuildingFieldStreaming::State::Reject(Cell &cell) {
  cell.Where = Phase::Rejected;
  cell.Groups.clear();
  cell.Alternatives.clear();
}

std::expected<bool, std::string>
BuildingFieldStreaming::State::PollGroup(Context &context, Tile &tile, Cell &cell, Group &group) {
  std::string error;
  if (group.GeometryRequest) {
    const auto ready = PollGeometry(context, group);
    if (!ready || !*ready) { return ready; }
  } else {
    if (!group.Request) { return false; }
    const auto ready = group.Request->Poll();
    if (!ready) { return std::unexpected(ready.error()); }
    if (!*ready) { return false; }
    if (!StageGroup(context, tile, group, **ready, error)) { return std::unexpected(error); }
    group.Metadata = (**ready).Metadata->Metadata();
    group.Support = (**ready).GeometricSupport;
  }
  if (!StageProduct(context, tile, cell, group, error)) { return std::unexpected(error); }
  group.Request.reset();
  group.Where = Phase::Ready;
  return true;
}

void BuildingFieldStreaming::State::PollCell(Context &context, Tile &tile, Cell &cell) {
  if (cell.Where != Phase::Pending) { return; }
  std::string error;
  for (auto &group : cell.Groups) {
    if (group.Where != Phase::Pending) { continue; }
    const auto ready = PollGroup(context, tile, cell, group);
    if (!ready) {
      error = ready.error();
      break;
    }
  }
  if (error.empty() &&
      std::ranges::all_of(cell.Groups, [](const Group &g) { return g.Where == Phase::Ready; }) &&
      Stage(context, tile, cell, error)) {
    cell.Where = Phase::Ready;
    cell.Alternatives.clear();
  }
  if (error.empty()) { return; }
  if (LastRejection != error) {
    Log::Warn(LogTag::World, "building_field_rejected", {{"reason", error}});
  }
  LastRejection = std::move(error);
  for (const auto &group : cell.Groups) {
    if (group.Where == Phase::Ready) {
      context.Pages.DiscardProduct(tile.Index, tile.SourceKey, group.AssetKey);
    }
  }
  if (cell.Alternatives.empty()) {
    Reject(cell);
  } else {
    cell.Groups = std::move(cell.Alternatives);
    cell.Alternatives.clear();
    cell.Where = Phase::Waiting;
  }
}

void BuildingFieldStreaming::State::PollFields(Context &context) {
  for (auto &tile : Tiles) {
    for (auto &cell : tile.Cells) { PollCell(context, tile, cell); }
  }
}

bool BuildingFieldStreaming::State::StageGroup(Context &context,
                                               const Tile &tile,
                                               Group &group,
                                               const BuildingFieldRequest::Product &product,
                                               std::string &error) {
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
  std::vector<uint32_t> triangles(indices.begin(), indices.end());
  auto material = context.Renderer.RegisterPieceMaterials(std::move(*geometry));
  if (!material) {
    error = material.error();
    return false;
  }
  group.Surface.emplace(Part{.Vertices = std::move(vertices),
                             .Tangents = std::move(tangents),
                             .Triangles = std::move(triangles),
                             .Colours = {},
                             .Uv1 = {},
                             .Material = std::move(*material)});
  group.AssetKey = product.Metadata->FieldKey({0});
  return true;
}

bool BuildingFieldStreaming::State::StageProduct(
    Context &context, const Tile &tile, const Cell &cell, Group &group, std::string &error) {
  std::vector<Render::GeometryPagePartView> parts;
  const auto append = [&](const Part &surface) {
    parts.push_back(
        {.Tangents = surface.Tangents,
         .Verts = surface.Vertices,
         .Indices = surface.Triangles,
         .Clusters = {},
         .Colours = surface.Colours,
         .Uv1 = surface.Uv1,
         .Surface = Render::PieceSurface::Registered(surface.Material, surface.MaterialIndex),
         .Textured = true,
         .Passes = Render::GeometryPasses::Main});
  };
  if (group.Surface) {
    append(*group.Surface);
  } else if (group.Native) {
    for (const auto &surface : group.Native->Parts) { append(surface); }
  }
  if (parts.empty() || !group.Coverage) {
    error = "building product has no complete surface";
    return false;
  }
  uint64_t digest = 0;
  (void)std::from_chars(group.AssetKey.data(), group.AssetKey.data() + 16, digest, 16);
  const Render::GeometryPageView page{.Parts = parts,
                                      .Digest = digest,
                                      .AssetKey = group.AssetKey,
                                      .FallbackHeights = false,
                                      .SelectedView = {},
                                      .RequestedDetail = LevelOfDetail::Skyline,
                                      .RequestedCell = cell.Source.Cell,
                                      .OccupiedCells = uint64_t{1} << (cell.Source.Cell - 1u),
                                      .Coverage = group.Coverage};
  if (!context.Pages.StageProduct(
          tile.Index, page, tile.Source.AnchorEcef, error, tile.SourceKey)) {
    return false;
  }
  group.Surface.reset();
  group.Native.reset();
  group.GeometryRequest.reset();
  return true;
}

bool BuildingFieldStreaming::State::Stage(Context &context,
                                          Tile &tile,
                                          Cell &cell,
                                          std::string &error) {
  std::vector<const SourceCoverage *> products;
  std::vector<std::string_view> keys;
  products.reserve(cell.Groups.size());
  keys.reserve(cell.Groups.size());
  std::string identity = "building-field-cell-2/" + tile.Source.BaseKey;
  for (const auto &group : cell.Groups) {
    products.push_back(group.Coverage ? &*group.Coverage : nullptr);
    keys.push_back(group.AssetKey);
    identity += "/" + group.AssetKey;
  }
  if (!SourceCoverage::Partition(cell.Coverage, products)) {
    error = "building groups do not partition their expected sources";
    return false;
  }
  std::ranges::sort(keys);
  cell.AssetKey = Sha256Hex(reinterpret_cast<const uint8_t *>(identity.data()), identity.size());
  uint64_t digest = 0;
  (void)std::from_chars(cell.AssetKey.data(), cell.AssetKey.data() + 16, digest, 16);
  const Render::GeometryPageView page{.Parts = {},
                                      .Digest = digest,
                                      .AssetKey = cell.AssetKey,
                                      .FallbackHeights = false,
                                      .SelectedView = {},
                                      .RequestedDetail = LevelOfDetail::Skyline,
                                      .RequestedCell = cell.Source.Cell,
                                      .OccupiedCells = uint64_t{1} << (cell.Source.Cell - 1u),
                                      .Coverage = cell.Coverage};
  cell.Staged = context.Pages.ConsolidateProducts(tile.Index, tile.SourceKey, page, keys, error);
  return cell.Staged;
}

bool BuildingFieldStreaming::State::ValidPose(const Context &context, const Group &group) {
  if (group.GeometryDetail) {
    const auto projection = context.Surface.Footprints().Projection();
    return *group.GeometryDetail == LevelOfDetail::Fine ||
           SourceProductSelection::GeometryErrorPx(
               group.Bounds, WorldCamera(context), projection.FocalPx, group.GeometryErrorM) <=
               projection.AllowedErrorPx;
  }
  if (!group.Support || !group.Metadata || !group.Metadata->WorldFromField) { return false; }
  const auto lens =
      Render::Lens::From(context.Camera, context.Renderer.PictureW(), context.Renderer.PictureH());
  const auto hull = Render::ImpostorPoseBounds::From(group.Support->Plane);
  if (!lens || !hull) { return false; }
  const auto camera = WorldCamera(context);
  const double radius = Length(group.Bounds.Span());
  const double distance = Length(camera.EyeM - group.Bounds.Middle()) - radius;
  const auto projection = context.Surface.Footprints().Projection();
  if (distance <= 0 || 4 * projection.FocalPx * radius / distance + 2 > group.Metadata->Pixels) {
    return false;
  }
  const auto bound = hull->Project(camera, *lens, *group.Metadata->WorldFromField);
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
    uint64_t coverage = 0;
    uint64_t active = 0;
    for (auto &choice : main) {
      coverage |= choice.Mask();
      const auto found =
          std::ranges::find(tile.Cells, choice.Cell, [](const Cell &c) { return c.Source.Cell; });
      if (key == tile.SourceKey && choice.Cell > 0 && choice.Cell <= 64 &&
          found != tile.Cells.end() && found->Where == Phase::Ready &&
          choice.Mask() == (uint64_t{1} << (choice.Cell - 1u)) && found->Staged &&
          std::ranges::all_of(found->Groups,
                              [&](const Group &g) { return ValidPose(context, g); })) {
        choice.ExpectedSources = &found->Coverage;
        choice.Detail = LevelOfDetail::Skyline;
        active |= choice.Mask();
      }
    }
    if (key == tile.SourceKey && coverage == tile.Source.Cells) { tile.Originals = original; }
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
