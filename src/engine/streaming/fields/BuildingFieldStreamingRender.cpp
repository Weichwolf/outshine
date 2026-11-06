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
#include <utility>
#include <vector>

namespace outshine {
namespace {
Box CaptureBounds(const BuildingFieldStreaming::Context &context,
                  const Render::RadiancePlane &plane,
                  const Mat4 &worldFromField) {
  const double extent = 3 * plane.HalfExtentM;
  const Vec3 half{{extent, extent, extent}};
  const Box capture{.Min = plane.Centre - half, .Max = plane.Centre + half};
  const auto world = capture.Through(worldFromField);
  Box scene;
  for (unsigned corner = 0; corner < 8; ++corner) {
    const auto local = context.Frame.ToLocalPosition(world.Corner(corner));
    scene.Cover(Vec3{{local.EastM, local.UpM, -local.NorthM}});
  }
  return scene;
}
}

void BuildingFieldStreaming::State::Reject(Cell &cell) {
  cell.Where = Phase::Rejected;
  cell.Groups.clear();
  cell.Alternatives.clear();
  cell.Consumer.reset();
}

void BuildingFieldStreaming::State::RecordAbortedPreflight(const Cell &cell) {
  if (cell.Where != Phase::Pending) { return; }
  Statistics.AvoidedRasterRequests += static_cast<size_t>(std::ranges::count_if(
      cell.Groups, [](const Group &group) { return group.Where == Phase::Prepared; }));
}

std::expected<void, std::string> BuildingFieldStreaming::State::PrepareCellMetadata(Cell &cell) {
  for (auto &group : cell.Groups) {
    if (group.Where != Phase::Pending || !group.Request) { continue; }
    const auto ready = group.Request->PollMetadata();
    if (!ready) { return std::unexpected(ready.error()); }
    if (*ready) { group.Where = Phase::Prepared; }
  }
  if (std::ranges::all_of(cell.Groups, [](const Group &g) { return g.Where == Phase::Prepared; })) {
    cell.Where = Phase::Prepared;
  }
  return {};
}

std::expected<void, std::string>
BuildingFieldStreaming::State::LoadCellFields(Context &context, Tile &tile, Cell &cell) {
  std::string error;
  for (auto &group : cell.Groups) {
    if (group.Where != Phase::Pending || !group.Request) { continue; }
    const auto ready = group.Request->Poll();
    if (!ready) { return std::unexpected(ready.error()); }
    if (!*ready) { continue; }
    if (!StageGroup(context, tile, group, **ready, error)) { return std::unexpected(error); }
    group.Metadata = (**ready).Metadata->Metadata();
    group.Support = (**ready).GeometricSupport;
    group.Request.reset();
    group.Where = Phase::Ready;
  }
  if (std::ranges::all_of(cell.Groups, [](const Group &g) { return g.Where == Phase::Ready; })) {
    if (!Stage(context, tile, cell, error)) { return std::unexpected(error); }
    cell.Where = Phase::Ready;
    cell.Alternatives.clear();
    cell.Consumer.reset();
    for (auto &group : cell.Groups) { group.Surface.reset(); }
  }
  return {};
}

void BuildingFieldStreaming::State::PollCell(Context &context, Tile &tile, Cell &cell) {
  if (cell.Where != Phase::Pending && cell.Where != Phase::Prepared) { return; }
  const auto result = cell.Where == Phase::Pending ? PrepareCellMetadata(cell)
                                                   : LoadCellFields(context, tile, cell);
  if (result) { return; }
  RecordAbortedPreflight(cell);
  const auto &error = result.error();
  if (LastRejection != error) {
    Log::Warn(LogTag::World, "building_field_rejected", {{"reason", error}});
  }
  LastRejection = error;
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
                             .Material = std::move(*material)});
  group.AssetKey = product.Metadata->FieldKey({0});
  group.SceneBounds = CaptureBounds(context, product.GeometricSupport->Plane, world);
  return true;
}

bool BuildingFieldStreaming::State::Stage(Context &context,
                                          Tile &tile,
                                          Cell &cell,
                                          std::string &error) {
  std::vector<const SourceCoverage *> products;
  products.reserve(cell.Groups.size());
  for (const auto &group : cell.Groups) {
    products.push_back(group.Coverage ? &*group.Coverage : nullptr);
  }
  if (!SourceCoverage::Partition(cell.Coverage, products)) {
    error = "building field groups do not partition their expected sources";
    return false;
  }
  std::vector<Render::GeometryPagePartView> parts;
  std::string identity = "building-field-cell-2/" + tile.Source.BaseKey;
  for (const auto &group : cell.Groups) {
    if (!group.Surface) {
      error = "building field group incomplete";
      return false;
    }
    const auto &surface = *group.Surface;
    parts.push_back({.Tangents = surface.Tangents,
                     .Verts = surface.Vertices,
                     .Indices = surface.Triangles,
                     .Clusters = {},
                     .Colours = {},
                     .Uv1 = {},
                     .Surface = Render::PieceSurface::Registered(surface.Material),
                     .Textured = true,
                     .Passes = Render::GeometryPasses::Main});
    identity += "/" + group.AssetKey;
  }
  cell.AssetKey = Sha256Hex(reinterpret_cast<const uint8_t *>(identity.data()), identity.size());
  uint64_t digest = 0;
  (void)std::from_chars(cell.AssetKey.data(), cell.AssetKey.data() + 16, digest, 16);
  const Render::GeometryPageView page{.Parts = parts,
                                      .Digest = digest,
                                      .AssetKey = cell.AssetKey,
                                      .FallbackHeights = false,
                                      .SelectedView = std::nullopt,
                                      .RequestedDetail = LevelOfDetail::Skyline,
                                      .RequestedCell = cell.Source.Cell,
                                      .OccupiedCells = uint64_t{1} << (cell.Source.Cell - 1u),
                                      .Coverage = cell.Coverage};
  cell.Staged =
      context.Pages.StageProduct(tile.Index, page, tile.Source.AnchorEcef, error, tile.SourceKey);
  return cell.Staged;
}

bool BuildingFieldStreaming::State::ValidPose(const Context &context, Group &group) {
  if (!group.Support || !group.Metadata || !group.Metadata->WorldFromField) { return false; }
  const auto lens =
      Render::Lens::From(context.Camera, context.Renderer.PictureW(), context.Renderer.PictureH());
  if (!lens) { return false; }
  const auto camera = WorldCamera(context);
  const double radius = Length(group.Bounds.Span());
  const double distance = Length(camera.EyeM - group.Bounds.Middle()) - radius;
  const auto projection = context.Surface.Footprints().Projection();
  if (distance <= 0 || 4 * projection.FocalPx * radius / distance + 2 > group.Metadata->Pixels) {
    return false;
  }
  auto &view = group.Support->View;
  if (view.Domain != BuildingFieldRequest::ViewDomain::Panoramic ||
      view.Space != BuildingFieldRequest::CoordinateSpace::World ||
      view.Camera.EyeM != camera.EyeM || view.Optics.WidePx != lens->WidePx ||
      view.Optics.HighPx != lens->HighPx ||
      !std::ranges::equal(view.Optics.Projection(), lens->Projection())) {
    const auto hull = Render::ImpostorPoseBounds::From(group.Support->Plane);
    if (!hull) { return false; }
    const auto bound = hull->ProjectPanoramic(camera.EyeM, *lens, *group.Metadata->WorldFromField);
    if (!bound) { return false; }
    group.Support->Bounds = *bound;
    view.Camera = camera;
    view.Optics = *lens;
    view.Domain = BuildingFieldRequest::ViewDomain::Panoramic;
    view.Space = BuildingFieldRequest::CoordinateSpace::World;
  }
  return group.Support->Bounds.DepthInside &&
         group.Support->Bounds.L1UpperPx <= projection.AllowedErrorPx;
}

}
