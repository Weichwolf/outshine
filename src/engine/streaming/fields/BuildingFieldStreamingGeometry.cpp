#include "BuildingFieldStreamingState.h"
#include "BuildingGeometrySource.h"
#include "SurfacePreparation.h"
#include "AssetProduct.h"
#include "SceneRenderer.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <stop_token>
#include <string>
#include <utility>

namespace outshine {
std::expected<BuildingFieldStreaming::State::PreparedGeometry, std::string>
BuildingFieldStreaming::State::PrepareGeometry(const GeometryAsset::Product &product,
                                               const Vec3 &anchor) {
  const auto &mesh = product.Mesh;
  if (mesh.images() != 0 || mesh.lamps() != 0) {
    return std::unexpected("building geometry contains unexpected image/light resources");
  }
  PreparedGeometry result;
  for (int at = 0; at < mesh.surfaces(); ++at) {
    if (!result.Materials.addSurface(mesh.surfaceNameOf(at),
                                     mesh.surfaceAt(MaterialInstance(at)))) {
      return std::unexpected("invalid native building material");
    }
  }
  result.Parts.reserve(static_cast<size_t>(mesh.parts()));
  for (int at = 0; at < mesh.parts(); ++at) {
    const auto positions = mesh.positionsOf(at);
    const auto normals = mesh.normalsOf(at);
    const auto texture = mesh.textureOf(at);
    const auto sourceTangents = mesh.tangentsOf(at);
    const auto count = positions.size() / 3;
    if (count == 0 || normals.size() != positions.size() || texture.size() != count * 2 ||
        (!sourceTangents.empty() && sourceTangents.size() != count * 4)) {
      return std::unexpected("native building vertex attributes are incomplete");
    }
    const Mat4 world = product.Layout.WorldFromLocal * mesh.placementOf(at);
    const Vec3 origin = world.TransformPoint({});
    const auto rotate = [&](const Vec3 &v) { return world.TransformPoint(v) - origin; };
    Part part;
    part.MaterialIndex = static_cast<uint32_t>(mesh.materialOf(at).index());
    part.Vertices.reserve(count);
    part.Tangents.reserve(sourceTangents.size());
    for (size_t vertex = 0; vertex < count; ++vertex) {
      const Vec3 p =
          world.TransformPoint(
              {{positions[vertex * 3], positions[vertex * 3 + 1], positions[vertex * 3 + 2]}}) -
          anchor;
      const Vec3 n =
          rotate({{normals[vertex * 3], normals[vertex * 3 + 1], normals[vertex * 3 + 2]}});
      part.Vertices.push_back(StoredVertex::Of(
          {{static_cast<float>(p[0]), static_cast<float>(p[1]), static_cast<float>(p[2])}},
          {{texture[vertex * 2], texture[vertex * 2 + 1]}},
          {{static_cast<float>(n[0]), static_cast<float>(n[1]), static_cast<float>(n[2])}}));
      if (!sourceTangents.empty()) {
        const Vec3 t = rotate({{sourceTangents[vertex * 4],
                                sourceTangents[vertex * 4 + 1],
                                sourceTangents[vertex * 4 + 2]}});
        part.Tangents.insert(part.Tangents.end(),
                             {static_cast<float>(t[0]),
                              static_cast<float>(t[1]),
                              static_cast<float>(t[2]),
                              sourceTangents[vertex * 4 + 3]});
      }
    }
    const auto triangles = mesh.trianglesOf(at);
    const auto colours = mesh.coloursOf(at);
    const auto uv1 = mesh.textureOf(at, UvSet::Uv1);
    part.Triangles.assign(triangles.begin(), triangles.end());
    part.Colours.assign(colours.begin(), colours.end());
    part.Uv1.assign(uv1.begin(), uv1.end());
    result.Parts.push_back(std::move(part));
  }
  return result;
}

bool BuildingFieldStreaming::State::RequestGeometry(Context &context,
                                                    Tile &tile,
                                                    Cell &cell,
                                                    Group &group) {
  if (!group.GeometryDetail) { return false; }
  const auto admission = context.Pipeline.Counts();
  if (group.Blocked == admission) { return false; }
  constexpr auto most = std::numeric_limits<size_t>::max();
  if (cell.Source.NativeBytes == 0 || cell.Source.NativeBytes > most / 128 ||
      cell.Source.HeaderBytes > most / 6) {
    Reject(cell);
    return false;
  }
  const size_t geometryBytes = cell.Source.NativeBytes * 16;
  const size_t consumers = geometryBytes * 4;
  if (cell.Source.HeaderBytes * 3 > most - consumers) {
    Reject(cell);
    return false;
  }
  if (!group.GeometryRecipe) {
    auto recipe = std::make_shared<Generators::BuildingGeometrySource::Recipe>();
    recipe->BaseKey = tile.Source.BaseKey;
    recipe->SourceCount = tile.SourceCount;
    recipe->Sources = std::move(group.Sources);
    recipe->Detail = *group.GeometryDetail;
    group.AssetKey = recipe->Key();
    group.GeometryRecipe = std::move(recipe);
  }
  const auto recipe = group.GeometryRecipe;
  const auto &key = group.AssetKey;
  if (key.empty()) {
    Reject(cell);
    return false;
  }
  const auto cache = context.Surface.AssetStore();
  const auto assets = context.Surface.BuildingAssets();
  auto prepared = std::make_shared<PreparedGeometry>();
  const Generators::BuildingGeometrySource::Budget limits{
      .EncodedBytesMost = geometryBytes,
      .TriangleAttempts = geometryBytes / (3 * sizeof(StoredVertex) + 3 * sizeof(uint32_t))};
  AssetRecord prototype{.Key = key,
                        .Kind = "native-geometry",
                        .Bounds = {},
                        .Package = key,
                        .ByteCount = 0,
                        .Parent = recipe->BaseKey};
  auto request = GeometryAssetRequest::Start(
      context.Pipeline,
      context.Workers,
      cache,
      key,
      recipe->BaseKey,
      {.ProducerBytes = cell.Source.HeaderBytes * 3 + consumers,
       .ConsumerBytes = consumers,
       .EncodedBytesMost = geometryBytes,
       .ReceiptBytesMost = AssetProduct(std::move(prototype)).CapacityBytes()},
      [cache, assets, recipe, limits](
          const std::stop_token &stop) -> std::expected<void, std::string> {
        const auto product = Generators::BuildingGeometrySource::Resolve(
            *cache, assets.get(), *recipe, limits, stop);
        if (!product) { return std::unexpected(product.error()); }
        return {};
      },
      [prepared, anchor = tile.Source.AnchorEcef](GeometryAsset::Product &product)
          -> std::expected<GeometryAssetRequest::Prepared, std::string> {
        auto decoded = PrepareGeometry(product, anchor);
        if (!decoded) { return std::unexpected(decoded.error()); }
        *prepared = std::move(*decoded);
        product.Mesh = Geometry{};
        return GeometryAssetRequest::Prepared{.Owner = prepared, .Bytes = prepared->HeapBytes()};
      });
  if (!request) {
    group.Blocked = context.Pipeline.Counts();
    return false;
  }
  group.GeometryRequest.emplace(std::move(*request));
  group.Native = std::move(prepared);
  group.Blocked.reset();
  group.Where = Phase::Pending;
  cell.Where = Phase::Pending;
  return true;
}

std::expected<bool, std::string> BuildingFieldStreaming::State::PollGeometry(Context &context,
                                                                             Group &group) {
  if (!group.GeometryRequest) {
    return std::unexpected("native building group has no geometry request");
  }
  const auto product = group.GeometryRequest->Poll();
  if (!product) { return std::unexpected(product.error()); }
  if (!*product) { return false; }
  if (!group.Native || group.Native->Parts.empty() || !group.Coverage ||
      (**product).Layout.Coverage != group.Coverage) {
    return std::unexpected("native building group has incomplete or foreign source coverage");
  }
  const auto material = context.Renderer.RegisterPieceMaterials(group.Native->Materials.clone());
  if (!material) { return std::unexpected(material.error()); }
  for (auto &part : group.Native->Parts) { part.Material = *material; }
  return true;
}
}
