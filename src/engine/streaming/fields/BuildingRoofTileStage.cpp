#include "BuildingFieldStreamingState.h"
#include "BuildingMaterials.h"
#include "ConvexPrism.h"
#include "GeometryPageView.h"
#include "Log.h"
#include "PieceMesh.h"
#include "SceneRenderer.h"
#include <array>
#include <bit>
#include <charconv>
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
Mat4 BuildingFieldStreaming::State::ToScene(const TangentFrame &frame, const Mat4 &world) {
  Mat4 result;
  for (size_t axis = 0; axis < 3; ++axis) {
    const auto d =
        frame.ToLocalDirection({{world.At(0, axis), world.At(1, axis), world.At(2, axis)}});
    result.At(0, axis) = d.EastM;
    result.At(1, axis) = d.UpM;
    result.At(2, axis) = -d.NorthM;
  }
  const auto p = frame.ToLocalPosition(world.Translation());
  result.SetTranslation({{p.EastM, p.UpM, -p.NorthM}});
  return result;
}

namespace {
Render::ConvexPrism ToPrism(const Generators::BuildingRoofProxy &roof,
                            const Vec3 &relativeAnchor,
                            const Mat4 &world,
                            const std::array<Material, 3> &materials) {
  constexpr float infinity = std::numeric_limits<float>::infinity();
  const auto wide = roof.WiderBounds();
  const auto centre = wide.Middle();
  const Vec3 offset = relativeAnchor + centre;
  Render::ConvexPrism prism{.SceneFromLocal = {},
                            .Bounds = {},
                            .Planes = roof.Planes,
                            .SecondaryFaces = (1u << 5u) | (1u << 6u)};
  for (size_t axis = 0; axis < 3; ++axis) {
    prism.Bounds.Min[axis] =
        std::nextafter(static_cast<float>(wide.Min[axis] - centre[axis]), -infinity);
    prism.Bounds.Max[axis] =
        std::nextafter(static_cast<float>(wide.Max[axis] - centre[axis]), infinity);
    for (size_t row = 0; row < 3; ++row) {
      prism.SceneFromLocal.At(row, axis) = static_cast<float>(world[row * 4 + axis]);
    }
    const Vec3 basis{{world.At(0, axis), world.At(1, axis), world.At(2, axis)}};
    prism.SceneFromLocal.At(axis, 3) = static_cast<float>(Dot(offset, basis));
    prism.Primary[axis] = roof.WallTint[axis];
    prism.Secondary[axis] = materials[1].BaseColour[axis] / materials[0].BaseColour[axis];
  }
  prism.Secondary[3] = materials[1].Roughness;
  return prism;
}

}

BuildingFieldStreaming::State::RemainderParts
BuildingFieldStreaming::State::MeshParts(const GeometryAsset::Product &asset,
                                         const Render::MaterialRegistration &materials) {
  RemainderParts result;
  const auto &mesh = asset.Mesh;
  result.Vertices.reserve(static_cast<size_t>(mesh.parts()));
  result.Parts.reserve(static_cast<size_t>(mesh.parts()));
  for (int i = 0; i < mesh.parts(); ++i) {
    const auto positions = mesh.positionsOf(i);
    const auto normals = mesh.normalsOf(i);
    const auto uv = mesh.textureOf(i);
    auto &vertices = result.Vertices.emplace_back();
    vertices.reserve(positions.size() / 3);
    for (size_t v = 0; v < positions.size() / 3; ++v) {
      vertices.push_back(
          StoredVertex::Of({{positions[3 * v], positions[3 * v + 1], positions[3 * v + 2]}},
                           {{uv[2 * v], uv[2 * v + 1]}},
                           {{normals[3 * v], normals[3 * v + 1], normals[3 * v + 2]}}));
    }
    result.Parts.push_back({.Tangents = mesh.tangentsOf(i),
                            .Verts = vertices,
                            .Indices = mesh.trianglesOf(i),
                            .Clusters = asset.Clusters[static_cast<size_t>(i)],
                            .Colours = mesh.coloursOf(i),
                            .Uv1 = mesh.textureOf(i, Geometry::UvSet::Uv1),
                            .Surface = Render::PieceSurface::Registered(
                                materials, static_cast<uint32_t>(mesh.materialOf(i).index() + 1)),
                            .Textured = true,
                            .Passes = Render::GeometryPasses::Main});
  }
  return result;
}

std::expected<Render::MaterialRegistration, std::string>
BuildingFieldStreaming::State::RegisterPaint(Render::SceneRenderer &renderer,
                                             const std::array<Material, 3> &surfaces) {
  Geometry palette;
  auto plain = surfaces.front();
  plain.Pattern = SurfacePattern::None;
  if (!palette.addSurface("distant-building", plain)) {
    return std::unexpected("invalid analytic building paint");
  }
  for (const auto &material : surfaces) {
    if (!palette.addSurface("building-mesh", material)) {
      return std::unexpected("invalid original building material");
    }
  }
  return renderer.RegisterPieceMaterials(std::move(palette));
}

namespace {
bool StageCaster(BuildingFieldStreaming::Context &context,
                 const BuildingRoofTile &product,
                 const Generators::BuildingShadowCell &caster,
                 const Render::MaterialRegistration &materials,
                 uint32_t tile,
                 uint64_t sourceKey) {
  const auto surface = Render::PieceSurface::Registered(materials, 1u);
  std::array<Render::GeometryPagePartView, 2> parts{};
  for (size_t at = 0; at < parts.size(); ++at) {
    parts[at] = {.Tangents = {},
                 .Verts = caster.Parts[at].Vertices,
                 .Indices = caster.Parts[at].Indices,
                 .Clusters = {},
                 .Colours = {},
                 .Uv1 = {},
                 .Surface = surface,
                 .Passes = Render::GeometryPasses::Shadow};
  }
  const auto cell = static_cast<uint32_t>(std::countr_zero(product.Cells)) + 1u;
  const Render::GeometryPageView view{.Parts = parts,
                                      .Digest = caster.Digest,
                                      .AssetKey = caster.Key,
                                      .SelectedView = std::nullopt,
                                      .RequestedDetail = LevelOfDetail::Skyline,
                                      .RequestedCell = cell,
                                      .OccupiedCells = product.Cells,
                                      .Coverage = product.Coverage};
  return context.Pages.StageProduct(tile, view, product.Anchor, sourceKey).has_value();
}
}

void BuildingFieldStreaming::State::PollRoof(Context &context, Tile &tile) {
  if (!tile.RoofUpload || !tile.RoofUpload->Coverage) { return; }
  auto &product = *tile.RoofUpload;
  const auto surfaces = Generators::BuildingSurfaceMaterials();
  const auto palette = RegisterPaint(context.Renderer, surfaces);
  if (!palette) {
    Log::Warn(LogTag::World, "building_roof_material_rejected", {{"reason", palette.error()}});
    tile.RoofUpload.reset();
    return;
  }
  Mat4 world;
  world.SetTranslation(product.Anchor);
  if (product.Remainder) { world = product.Remainder->Layout.WorldFromLocal; }
  const auto placement = std::make_shared<const Mat4>(ToScene(context.Frame, world));
  std::vector<Render::ConvexPrism> prisms;
  prisms.reserve(product.Roofs.size());
  const auto relativeAnchor = product.Anchor - world.Translation();
  for (const auto &roof : product.Roofs) {
    prisms.push_back(ToPrism(roof, relativeAnchor, world, surfaces));
  }
  const auto remainder =
      product.Remainder ? MeshParts(*product.Remainder, *palette) : RemainderParts{};
  uint64_t digest = 0;
  (void)std::from_chars(product.Key.data(), product.Key.data() + 16, digest, 16);
  const Render::GeometryPageView page{
      .Parts = remainder.Parts,
      .Digest = digest,
      .AssetKey = product.Key,
      .SelectedView = std::nullopt,
      .RequestedDetail = LevelOfDetail::Skyline,
      .RequestedCell = static_cast<uint32_t>(std::countr_zero(product.Cells)) + 1u,
      .OccupiedCells = product.Cells,
      .Coverage = product.Coverage,
      .Placement = placement,
      .Prism = Render::GeometryPagePrismView{
          .Shapes = prisms, .Material = *palette, .Passes = Render::GeometryPasses::Main}};
  const auto staged = context.Pages.StageProduct(tile.Index, page, product.Anchor, tile.SourceKey);
  if (!staged && staged.error().Kind == Render::Native::ErrorKind::Busy) { return; }
  if (staged) {
    std::optional<Tile::RoofProduct::Shadow> shadow;
    if (product.Caster &&
        StageCaster(context, product, *product.Caster, *palette, tile.Index, tile.SourceKey)) {
      auto &caster = *product.Caster;
      shadow.emplace(Tile::RoofProduct::Shadow{.Key = std::move(caster.Key),
                                               .Error = {.SourceKey = tile.SourceKey,
                                                         .Bounds = caster.Bounds,
                                                         .UpperM = caster.ErrorM}});
    }
    tile.ResidentRoof.emplace(Tile::RoofProduct{.Projected = product.Projected,
                                                .Key = std::move(product.Key),
                                                .Coverage = std::move(*product.Coverage),
                                                .Cells = product.Cells,
                                                .Count = product.Roofs.size(),
                                                .Caster = std::move(shadow)});
  } else {
    Log::Warn(
        LogTag::World, "building_roof_product_rejected", {{"reason", staged.error().Message}});
  }
  tile.RoofUpload.reset();
}
}
