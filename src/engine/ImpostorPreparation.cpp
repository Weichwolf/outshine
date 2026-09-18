#include "math/Srgb.h"
#include "ImpostorPreparation.h"
#include "StoredVertex.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "../../build/CrownBuild.h"
#include "Digest.h"
#include <bit>
#include <type_traits>
#include "Live.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <array>

namespace outshine {
namespace {
constexpr double kCaptureIlluminanceLux = 20000;
constexpr double kCaptureLightBearingDeg = 135;
constexpr double kCaptureLightElevationDeg = 40;
constexpr size_t kMostAtlasTexels = 1u << 24u;
constexpr auto kOpaqueByte = std::numeric_limits<uint8_t>::max();

namespace Says {
constexpr auto Shape =
    "crown atlas requires 3..4096 pixels, 1..64 views and at most 16777216 texels";
constexpr auto Geometry = "crown atlas requires native tree geometry";
constexpr auto Bounds = "crown atlas requires finite nonempty bounds";
constexpr auto Readback = "crown atlas readback has incomplete attachments";
constexpr auto Surface = "crown atlas material identity is outside its source table";
}

uint8_t Byte(float value) {
  return static_cast<uint8_t>(
      std::lround(std::clamp(value, 0.0f, 1.0f) * static_cast<float>(kOpaqueByte)));
}

}

std::string ImpostorAtlasProvenance(std::string_view species, Content::ImpostorAtlasShape shape) {
  return std::string(kCrownBuildIdentity) + "/" + std::to_string(shape.Pixels) + "/" +
         std::to_string(shape.Views) + "/" + std::string(species);
}

namespace {
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

namespace {
struct AtlasReadback {
  std::span<const float> Depth;
  std::span<const float> Normal;
  std::span<const float> Identity;
  size_t PixelCount;
  size_t MaterialCount;
};

std::optional<std::vector<Content::ImpostorAtlas::Texel>>
ConvertAtlasReadback(const AtlasReadback &readback, std::string &error) {
  const size_t count = readback.PixelCount;
  if (readback.Depth.size() != count || readback.Normal.size() != count * 4 ||
      readback.Identity.size() != count * 4) {
    error = Says::Readback;
    return std::nullopt;
  }
  std::vector<Content::ImpostorAtlas::Texel> texels(count);
  for (size_t pixel = 0; pixel < count; ++pixel) {
    const float surface = readback.Identity[pixel * 4];
    if (!std::isfinite(surface) || surface < 0 ||
        static_cast<double>(surface) > static_cast<double>(readback.MaterialCount) ||
        surface != std::floor(surface)) {
      error = Says::Surface;
      return std::nullopt;
    }
    texels[pixel] = {.Normal = {{readback.Normal[pixel * 4],
                                 readback.Normal[pixel * 4 + 1],
                                 readback.Normal[pixel * 4 + 2]}},
                     .Depth = readback.Depth[pixel],
                     .Surface = static_cast<uint32_t>(surface)};
  }
  return texels;
}
}

namespace {
struct PreparedAtlasGeometry {
  std::vector<Material> Surfaces;
  std::vector<StoredVertex> Vertices;
};

std::optional<PreparedAtlasGeometry>
PrepareAtlasGeometry(Geometry &base, std::string &error, const Geometry *leaf = nullptr) {
  if (leaf != nullptr && !base.addSurface("leaves", leaf->surfaceAt(MaterialInstance(0)))) {
    error = Says::Geometry;
    return std::nullopt;
  }
  PreparedAtlasGeometry prepared;
  for (int at = 0; at < base.surfaces(); ++at) {
    prepared.Surfaces.push_back(base.surfaceAt(MaterialInstance(at)));
  }
  if (leaf == nullptr) { return prepared; }
  const auto positions = leaf->positionsOf(0);
  const auto normals = leaf->normalsOf(0);
  const auto uv = leaf->textureOf(0);
  prepared.Vertices.resize(positions.size() / 3);
  for (size_t at = 0; at < prepared.Vertices.size(); ++at) {
    prepared.Vertices[at] =
        StoredVertex::Of({{positions[at * 3], positions[at * 3 + 1], positions[at * 3 + 2]}},
                         {{uv[at * 2], uv[at * 2 + 1]}},
                         {{normals[at * 3], normals[at * 3 + 1], normals[at * 3 + 2]}});
  }
  return prepared;
}
}

std::optional<Content::ImpostorAtlas> BakeImpostorAtlas(const Generators::TreePrototype &tree,
                                                        Content::ImpostorAtlasShape shape,
                                                        std::string &error) {
  if (shape.Pixels < 3 || shape.Pixels > 4096 || shape.Views == 0 || shape.Views > 64 ||
      static_cast<size_t>(shape.Pixels) * static_cast<size_t>(shape.Pixels) * shape.Views >
          kMostAtlasTexels) {
    error = Says::Shape;
    return std::nullopt;
  }
  auto geometry = tree.InstancedGeometryAt(0);
  if (!geometry) {
    error = Says::Geometry;
    return std::nullopt;
  }
  const Vec3 least = geometry->LeastM;
  const Vec3 most = geometry->MostM;
  const Vec3 centre = (least + most) * 0.5;
  const Vec3 half = (most - least) * 0.5;
  const double halfExtent = std::max(std::hypot(half[0], half[2]), half[1]) *
                            static_cast<double>(shape.Pixels) /
                            static_cast<double>(shape.Pixels - 2);
  if (!(halfExtent > 0.0) || !std::isfinite(halfExtent)) {
    error = Says::Bounds;
    return std::nullopt;
  }
  const bool leaves = !geometry->Placements.empty() && geometry->Leaf.parts() > 0;
  auto prepared = PrepareAtlasGeometry(geometry->Bark, error, leaves ? &geometry->Leaf : nullptr);
  if (!prepared) { return std::nullopt; }
  std::vector<Content::ImpostorAtlas::View> views;
  views.reserve(shape.Views);
  std::vector<float> depth;
  std::vector<float> normal;
  std::vector<float> identity;
  const size_t count = static_cast<size_t>(shape.Pixels) * static_cast<size_t>(shape.Pixels);
  for (unsigned at = 0; at < shape.Views; ++at) {
    const double angle = 2 * std::numbers::pi * at / shape.Views;
    const Vec3 direction{{std::sin(angle), 0, std::cos(angle)}};
    views.push_back({.TowardEye = direction, .Texels = {}});
    Render::SceneRenderer renderer;
    Core::Declaration declaration;
    declaration.InitialGeometry = &geometry->Bark;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = shape.Pixels;
    declaration.Outputs = {"sceneDepth", "sceneShadingNormal", "sceneSurfaceIdentity"};
    declaration.KeyLux = kCaptureIlluminanceLux;
    declaration.KeyBearingDeg = kCaptureLightBearingDeg;
    declaration.KeyElevationDeg = kCaptureLightElevationDeg;
    std::unique_ptr<Core::Live> live;
    if (!Core::Live::Open(renderer, declaration, nullptr, live, error)) { return std::nullopt; }
    auto camera = Render::Viewpoint::LookAt(
        {.EyeM = centre + direction * (3 * halfExtent), .AimM = centre}, 0);
    if (!camera) {
      error = Says::Bounds;
      return std::nullopt;
    }
    camera->Kind = Render::CameraKind::Orthographic;
    camera->XMagM = camera->YMagM = halfExtent;
    camera->ZNearM = halfExtent;
    camera->ZFarM = 5 * halfExtent;
    live->Eye(*camera);
    if (leaves) {
      Render::PieceMesh piece;
      piece.Verts = prepared->Vertices;
      piece.Indices = geometry->Leaf.trianglesOf(0);
      piece.Instances = geometry->Placements;
      piece.MaxInstances = static_cast<uint32_t>(geometry->Placements.size());
      piece.Surface = Render::PieceSurface(1);
      piece.Textured = true;
      if (auto placed = live->PlacePiece(piece); !placed) {
        error = std::move(placed.error());
        return std::nullopt;
      }
    }
    if (!live->Draw(error)) { return std::nullopt; }
    renderer.WaitForGpu();
    if (renderer.ReadDepth(depth) != Render::ReadState::Ready ||
        renderer.ReadShadingNormal(normal) != Render::ReadState::Ready ||
        renderer.ReadSurfaceIdentity(identity) != Render::ReadState::Ready) {
      error = Says::Readback;
      return std::nullopt;
    }
    auto texels = ConvertAtlasReadback({.Depth = depth,
                                        .Normal = normal,
                                        .Identity = identity,
                                        .PixelCount = count,
                                        .MaterialCount = prepared->Surfaces.size()},
                                       error);
    if (!texels) { return std::nullopt; }
    views[at].Texels = std::move(*texels);
  }
  return Content::ImpostorAtlas::Create(
      shape.Pixels, centre, halfExtent, std::move(prepared->Surfaces), std::move(views), error);
}

}
