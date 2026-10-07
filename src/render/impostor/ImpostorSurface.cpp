#include "ImpostorSurface.h"
#include "DepthSurfacePatches.h"

#include "math/Srgb.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <string>
#include <vector>
#include <utility>

namespace outshine::Render {
namespace {
constexpr auto kOpaqueByte = std::numeric_limits<uint8_t>::max();
constexpr double kDepthErrorTexelDivisor = 16.0;

uint8_t Byte(float value) {
  return static_cast<uint8_t>(
      std::lround(std::clamp(value, 0.0f, 1.0f) * static_cast<float>(kOpaqueByte)));
}

std::vector<uint32_t> NearestCoveredTexels(const Content::ImpostorAtlas::View &source, int pixels) {
  const size_t count = source.Texels.size();
  constexpr uint32_t missing = std::numeric_limits<uint32_t>::max();
  std::vector<uint32_t> owner(count, missing);
  std::vector<uint32_t> queue;
  queue.reserve(count);
  for (uint32_t at = 0; at < count; ++at) {
    if (source.Texels[at].Surface == 0) { continue; }
    owner[at] = at;
    queue.push_back(at);
  }
  if (queue.empty()) { return {}; }
  const auto width = static_cast<uint32_t>(pixels);
  for (size_t next = 0; next < queue.size(); ++next) {
    const uint32_t at = queue[next];
    const auto extend = [&](uint32_t to) {
      if (owner[to] != missing) { return; }
      owner[to] = owner[at];
      queue.push_back(to);
    };
    if (at % width > 0) { extend(at - 1); }
    if (at % width + 1 < width) { extend(at + 1); }
    if (at >= width) { extend(at - width); }
    if (at + width < count) { extend(at + width); }
  }
  return owner;
}

std::expected<std::vector<DepthSurfacePatch>, DepthSurfaceError>
PatchesOf(const Content::ImpostorAtlas &atlas,
          const Content::ImpostorAtlas::View &source,
          ImpostorSurfaceDetail detail) {
  const auto pixels = static_cast<uint32_t>(atlas.Pixels());
  if (detail == ImpostorSurfaceDetail::Flat) {
    return std::vector<DepthSurfacePatch>{{.Right = pixels, .Bottom = pixels, .OriginDepth = 0.5}};
  }
  const size_t count = source.Texels.size();
  std::vector<float> depths(count);
  std::vector<uint32_t> surfaces(count);
  for (size_t at = 0; at < count; ++at) {
    depths[at] = source.Texels[at].Depth;
    surfaces[at] = source.Texels[at].Surface;
  }
  return BuildDepthSurfacePatches({.Width = pixels,
                                   .Height = pixels,
                                   .Depth = depths,
                                   .Surface = surfaces,
                                   .AllowedError = 1.0 / (kDepthErrorTexelDivisor * pixels)});
}

bool AddDepthSurface(Geometry &geometry,
                     int part,
                     const Content::ImpostorAtlas &atlas,
                     const Content::ImpostorAtlas::View &source,
                     Vec3 toward,
                     Vec3 right,
                     ImpostorSurfaceDetail detail) {
  const Vec3 centre = atlas.CentreM();
  const double halfExtent = atlas.HalfExtentM();
  const auto pixels = static_cast<uint32_t>(atlas.Pixels());
  const auto patches = PatchesOf(atlas, source, detail);
  if (!patches || patches->size() > std::numeric_limits<uint32_t>::max() / 4u) { return false; }
  std::vector<float> positions;
  std::vector<float> normals;
  std::vector<float> tangents;
  std::vector<float> uv;
  std::vector<uint32_t> triangles;
  positions.reserve(patches->size() * 12);
  normals.reserve(patches->size() * 12);
  tangents.reserve(patches->size() * 16);
  uv.reserve(patches->size() * 8);
  triangles.reserve(patches->size() * 6);
  for (const auto &patch : *patches) {
    const auto first = static_cast<uint32_t>(positions.size() / 3);
    const std::array<std::pair<uint32_t, uint32_t>, 4> corners{{{patch.Left, patch.Bottom},
                                                                {patch.Right, patch.Bottom},
                                                                {patch.Right, patch.Top},
                                                                {patch.Left, patch.Top}}};
    for (const auto [pixelX, pixelY] : corners) {
      const double u = static_cast<double>(pixelX) / pixels;
      const double v = static_cast<double>(pixelY) / pixels;
      const Vec3 point = centre + right * ((2.0 * u - 1.0) * halfExtent) +
                         Vec3{{0, (1.0 - 2.0 * v) * halfExtent, 0}} +
                         toward * ((4.0 * patch.At(pixelX, pixelY) - 2.0) * halfExtent);
      for (size_t axis = 0; axis < 3; ++axis) {
        positions.push_back(static_cast<float>(point[axis]));
        normals.push_back(static_cast<float>(toward[axis]));
        tangents.push_back(static_cast<float>(right[axis]));
      }
      tangents.push_back(-1);
      uv.push_back(static_cast<float>(u));
      uv.push_back(static_cast<float>(v));
    }
    for (const uint32_t offset : {0u, 1u, 2u, 0u, 2u, 3u}) { triangles.push_back(first + offset); }
  }
  return geometry.setPositions(part, positions) && geometry.setNormals(part, normals) &&
         geometry.setTangents(part, tangents) && geometry.setTexture(part, uv) &&
         geometry.setTriangles(part, triangles);
}
}

std::optional<Geometry> BuildImpostorSurface(const Content::ImpostorAtlas &atlas,
                                             size_t view,
                                             ImpostorSurfaceDetail detail) {
  if (view >= atlas.Views().size()) { return std::nullopt; }
  const Content::ImpostorAtlas::View &source = atlas.Views()[view];
  const size_t count = source.Texels.size();
  const auto owner = NearestCoveredTexels(source, atlas.Pixels());
  if (owner.empty()) { return std::nullopt; }
  const Vec3 toward = source.TowardEye;
  const Vec3 right{{toward[2], 0, -toward[0]}};
  std::array<std::vector<uint8_t>, 3> images;
  for (auto &image : images) { image.resize(count * 4); }
  for (size_t at = 0; at < count; ++at) {
    const Content::ImpostorAtlas::Texel &pixel = source.Texels[owner[at]];
    const Material &material = atlas.Surfaces()[pixel.Surface - 1];
    const auto sample =
        source.Materials.empty()
            ? Content::ImpostorAtlas::SurfaceSample{.BaseColour = {{material.BaseColour[0],
                                                                    material.BaseColour[1],
                                                                    material.BaseColour[2]}},
                                                    .Roughness = material.Roughness,
                                                    .Metalness = material.Metalness}
            : source.Materials[owner[at]];
    Vec3 n{{pixel.Normal[0], pixel.Normal[1], pixel.Normal[2]}};
    if (!Normalise(n)) { return std::nullopt; }
    for (size_t c = 0; c < 3; ++c) {
      images[0][at * 4 + c] = Byte(ColourSpace::SrgbFromLinear(sample.BaseColour[c]));
    }
    images[0][at * 4 + 3] = source.Texels[at].Surface > 0 ? kOpaqueByte : 0;
    images[1][at * 4] = Byte(static_cast<float>(0.5 * (Dot(n, right) + 1.0)));
    images[1][at * 4 + 1] = Byte(static_cast<float>(0.5 * (1.0 - n[1])));
    images[1][at * 4 + 2] = Byte(static_cast<float>(0.5 * (Dot(n, toward) + 1.0)));
    images[1][at * 4 + 3] = kOpaqueByte;
    images[2][at * 4] = kOpaqueByte;
    images[2][at * 4 + 1] = Byte(sample.Roughness);
    images[2][at * 4 + 2] = Byte(sample.Metalness);
    images[2][at * 4 + 3] = kOpaqueByte;
  }
  Geometry geometry;
  Material material;
  material.BaseColour = {{1, 1, 1, 1}};
  material.Metalness = material.Roughness = 1;
  material.Alpha = AlphaMode::Masked;
  std::array<SurfaceMap *, 3> maps{
      &material.BaseColourMap, &material.NormalMap, &material.MetalRoughMap};
  for (size_t at = 0; at < maps.size(); ++at) {
    const auto image = geometry.addImage(atlas.Pixels(), atlas.Pixels(), images[at]);
    if (!image) { return std::nullopt; }
    maps[at]->Image = *image;
    maps[at]->Sampler.WrapU = maps[at]->Sampler.WrapV = Wrap::ClampToEdge;
  }
  const auto surface = geometry.addSurface("impostor", material);
  if (!surface) { return std::nullopt; }
  const auto createdPart = geometry.addPart("impostor", *surface);
  if (!createdPart) { return std::nullopt; }
  const int part = *createdPart;
  if (!AddDepthSurface(geometry, part, atlas, source, toward, right, detail)) {
    return std::nullopt;
  }
  return geometry;
}

std::optional<Content::ImpostorCards> PrepareImpostorCards(const Content::ImpostorAtlas &atlas,
                                                           std::string &error) {
  Content::ImpostorCards cards;
  cards.Centre = atlas.CentreM();
  cards.HalfExtentM = atlas.HalfExtentM();
  cards.Views.reserve(atlas.Views().size());
  for (size_t view = 0; view < atlas.Views().size(); ++view) {
    auto geometry = BuildImpostorSurface(atlas, view, ImpostorSurfaceDetail::Flat);
    if (!geometry) {
      error = "impostor view has no native card geometry";
      return std::nullopt;
    }
    cards.Views.push_back(
        {.Direction = atlas.Views()[view].TowardEye, .Surface = std::move(*geometry)});
  }
  return cards;
}

}
