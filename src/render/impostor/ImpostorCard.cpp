#include "ImpostorCard.h"

#include "math/Srgb.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace outshine::Render {
namespace {
constexpr auto kOpaqueByte = std::numeric_limits<uint8_t>::max();

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
}

std::optional<Geometry> BuildImpostorCard(const Content::ImpostorAtlas &atlas, size_t view) {
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
    Vec3 n{{pixel.Normal[0], pixel.Normal[1], pixel.Normal[2]}};
    if (!Normalise(n)) { return std::nullopt; }
    for (size_t c = 0; c < 3; ++c) {
      images[0][at * 4 + c] = Byte(ColourSpace::SrgbFromLinear(material.BaseColour[c]));
    }
    images[0][at * 4 + 3] = source.Texels[at].Surface > 0 ? kOpaqueByte : 0;
    images[1][at * 4] = Byte(static_cast<float>(0.5 * (Dot(n, right) + 1.0)));
    images[1][at * 4 + 1] = Byte(static_cast<float>(0.5 * (1.0 - n[1])));
    images[1][at * 4 + 2] = Byte(static_cast<float>(0.5 * (Dot(n, toward) + 1.0)));
    images[1][at * 4 + 3] = kOpaqueByte;
    images[2][at * 4] = kOpaqueByte;
    images[2][at * 4 + 1] = Byte(material.Roughness);
    images[2][at * 4 + 2] = Byte(material.Metalness);
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
  const auto surface = geometry.addSurface("crown", material);
  if (!surface) { return std::nullopt; }
  const auto createdPart = geometry.addPart("crown", *surface);
  if (!createdPart) { return std::nullopt; }
  const int part = *createdPart;
  const Vec3 centre = atlas.CentreM();
  const double halfExtent = atlas.HalfExtentM();
  const std::array<Vec3, 4> corners{centre - right * halfExtent - Vec3{{0, halfExtent, 0}},
                                    centre + right * halfExtent - Vec3{{0, halfExtent, 0}},
                                    centre + right * halfExtent + Vec3{{0, halfExtent, 0}},
                                    centre - right * halfExtent + Vec3{{0, halfExtent, 0}}};
  std::array<float, 12> positions;
  std::array<float, 12> normals;
  std::array<float, 16> tangents;
  for (size_t at = 0; at < corners.size(); ++at) {
    for (size_t axis = 0; axis < 3; ++axis) {
      positions[at * 3 + axis] = static_cast<float>(corners[at][axis]);
      normals[at * 3 + axis] = static_cast<float>(toward[axis]);
      tangents[at * 4 + axis] = static_cast<float>(right[axis]);
    }
    tangents[at * 4 + 3] = -1;
  }
  if (part < 0 || !geometry.setPositions(part, positions) || !geometry.setNormals(part, normals) ||
      !geometry.setTangents(part, tangents) ||
      !geometry.setTexture(part, std::array<float, 8>{0, 1, 1, 1, 1, 0, 0, 0}) ||
      !geometry.setTriangles(part, std::array<uint32_t, 6>{0, 1, 2, 0, 2, 3})) {
    return std::nullopt;
  }
  return geometry;
}

}
