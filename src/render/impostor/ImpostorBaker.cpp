#include "ImpostorBaker.h"

#include "AzimuthElevation.h"
#include "Compiled.h"
#include "Lens.h"
#include "SceneRenderer.h"
#include "Shape.h"
#include "StoredVertex.h"
#include "math/Units.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Render {
namespace {
constexpr double kCaptureIlluminanceLux = 20000;
constexpr double kCaptureLightBearingDeg = 135;
constexpr double kCaptureLightElevationDeg = 40;
constexpr size_t kMostAtlasTexels = 1u << 24u;
constexpr float kCapturedNormalSquaredTolerance = 0.003f;

namespace Says {
constexpr auto Shape =
    "impostor atlas requires 3..4096 pixels, 1..64 views and at most 16777216 texels";
constexpr auto Geometry = "impostor capture requires complete native geometry and placements";
constexpr auto Bounds = "impostor capture requires finite nonempty bounds";
constexpr auto Readback = "impostor atlas readback has incomplete attachments";
constexpr auto Surface = "impostor atlas material identity is outside its source table";
}

struct AtlasReadback {
  std::span<const float> Depth;
  std::span<const float> Normal;
  std::span<const float> Identity;
  size_t PixelCount;
  size_t MaterialCount;
};

struct NativePiece {
  std::vector<StoredVertex> Vertices;
  std::vector<uint32_t> Indices;
  std::vector<float> Tangents;
  std::vector<float> Colours;
};

std::optional<std::vector<Content::ImpostorAtlas::Texel>>
ConvertReadback(const AtlasReadback &readback, std::string &error) {
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

std::optional<NativePiece> BuildPiece(const ShapePart &part,
                                      std::span<const uint32_t> indices,
                                      bool mapped,
                                      std::string &error) {
  if (part.PositionsM.size() != part.VertexCount * 3 ||
      part.Normals.size() != part.VertexCount * 3 ||
      (!part.Uv.empty() && part.Uv.size() != part.VertexCount * 2) ||
      part.FirstIndex > indices.size() || part.IndexCount > indices.size() - part.FirstIndex) {
    error = Says::Geometry;
    return std::nullopt;
  }
  NativePiece piece;
  piece.Vertices.resize(part.VertexCount);
  for (size_t at = 0; at < part.VertexCount; ++at) {
    const Vec2f uv = part.Uv.empty() ? Vec2f{} : Vec2f{{part.Uv[at * 2], part.Uv[at * 2 + 1]}};
    piece.Vertices[at] = StoredVertex::Of(
        {{part.PositionsM[at * 3], part.PositionsM[at * 3 + 1], part.PositionsM[at * 3 + 2]}},
        uv,
        {{part.Normals[at * 3], part.Normals[at * 3 + 1], part.Normals[at * 3 + 2]}});
  }
  piece.Indices.assign(indices.begin() + static_cast<ptrdiff_t>(part.FirstIndex),
                       indices.begin() + static_cast<ptrdiff_t>(part.FirstIndex + part.IndexCount));
  for (uint32_t &index : piece.Indices) {
    if (index < part.FirstVertex || index - part.FirstVertex >= part.VertexCount) {
      error = Says::Geometry;
      return std::nullopt;
    }
    index -= static_cast<uint32_t>(part.FirstVertex);
  }
  if (mapped) { piece.Tangents.assign(part.Tangents.begin(), part.Tangents.end()); }
  piece.Colours.assign(part.Colours.begin(), part.Colours.end());
  return piece;
}

bool InstallSource(SceneRenderer &renderer,
                   ImpostorCaptureSource source,
                   std::vector<Material> &surfaces,
                   std::string &error) {
  if (!source.Mesh.wellFormed() || source.Mesh.parts() == 0 || source.Mesh.surfaces() == 0 ||
      source.Instances.empty() || source.Instances.size() > std::numeric_limits<uint32_t>::max()) {
    error = Says::Geometry;
    return false;
  }
  ShapeStore storage;
  const auto shaped = PrepareShape(source.Mesh, storage);
  if (!shaped) {
    error = Describe(shaped.error());
    return false;
  }
  for (int at = 0; at < source.Mesh.surfaces(); ++at) {
    surfaces.push_back(source.Mesh.surfaceAt(MaterialInstance(at)));
  }
  auto firstSurface = renderer.RegisterPieceMaterials(std::move(source.Mesh));
  if (!firstSurface) {
    error = std::move(firstSurface).error();
    return false;
  }
  for (const ShapePart &part : shaped->Parts) {
    if (part.Material < 0 || static_cast<size_t>(part.Material) >= shaped->Surfaces.size()) {
      error = Says::Geometry;
      return false;
    }
    const Material &material = shaped->Surfaces[static_cast<size_t>(part.Material)];
    auto native = BuildPiece(part, shaped->Indices, material.NormalMap.bound(), error);
    if (!native) { return false; }
    PieceMesh piece;
    piece.Verts = native->Vertices;
    piece.Indices = native->Indices;
    piece.Tangents = native->Tangents;
    piece.Colours = native->Colours;
    piece.Instances = source.Instances;
    piece.MaxInstances = static_cast<uint32_t>(source.Instances.size());
    piece.Surface = PieceSurface::Registered(*firstSurface + static_cast<uint32_t>(part.Material));
    piece.Textured = !part.Uv.empty();
    if (auto placed = renderer.PlacePiece(piece); !placed) {
      error = std::move(placed.error());
      return false;
    }
  }
  return true;
}

std::optional<std::vector<Content::ImpostorAtlas::View>>
CaptureViews(SceneRenderer &renderer,
             const Vec3 &centre,
             double halfExtent,
             Content::ImpostorAtlasShape shape,
             size_t materialCount,
             std::string &error) {
  std::vector<Content::ImpostorAtlas::View> views;
  views.reserve(shape.Views);
  std::vector<float> depth;
  std::vector<float> normal;
  std::vector<float> identity;
  const size_t count = static_cast<size_t>(shape.Pixels) * static_cast<size_t>(shape.Pixels);
  for (unsigned at = 0; at < shape.Views; ++at) {
    const double angle = 2 * std::numbers::pi * at / shape.Views;
    const Vec3 direction{{std::sin(angle), 0, std::cos(angle)}};
    auto camera =
        Viewpoint::LookAt({.EyeM = centre + direction * (3 * halfExtent), .AimM = centre}, 0);
    if (!camera) {
      error = Says::Bounds;
      return std::nullopt;
    }
    camera->Kind = CameraKind::Orthographic;
    camera->XMagM = camera->YMagM = halfExtent;
    camera->ZNearM = halfExtent;
    camera->ZFarM = 5 * halfExtent;
    const auto lens = Lens::From(*camera, shape.Pixels, shape.Pixels);
    if (!lens) {
      error = Says::Bounds;
      return std::nullopt;
    }
    renderer.SetCamera(*camera, *lens);
    if (auto rendered = renderer.RenderFrame(); !rendered) {
      error = std::move(rendered.error());
      return std::nullopt;
    }
    renderer.WaitForGpu();
    if (renderer.ReadDepth(depth) != ReadState::Ready ||
        renderer.ReadShadingNormal(normal) != ReadState::Ready ||
        renderer.ReadSurfaceIdentity(identity) != ReadState::Ready) {
      error = Says::Readback;
      return std::nullopt;
    }
    auto texels = ConvertReadback({.Depth = depth,
                                   .Normal = normal,
                                   .Identity = identity,
                                   .PixelCount = count,
                                   .MaterialCount = materialCount},
                                  error);
    if (!texels) { return std::nullopt; }
    views.push_back({.TowardEye = direction, .Texels = std::move(*texels)});
  }
  for (size_t view = 0; view < views.size(); ++view) {
    for (size_t pixel = 0; pixel < views[view].Texels.size(); ++pixel) {
      const auto &texel = views[view].Texels[pixel];
      const float length = Dot(texel.Normal, texel.Normal);
      if (!std::isfinite(texel.Depth) || texel.Depth < 0 || texel.Depth > 1 ||
          ((texel.Surface > 0) != (texel.Depth > 0)) || !std::isfinite(length) ||
          (texel.Surface > 0 ? std::abs(length - 1) > kCapturedNormalSquaredTolerance
                             : length != 0)) {
        error = "impostor capture produced an invalid sample at view " + std::to_string(view) +
                " pixel " + std::to_string(pixel) + " with depth " + std::to_string(texel.Depth) +
                ", surface " + std::to_string(texel.Surface) + ", normal (" +
                std::to_string(texel.Normal[0]) + ", " + std::to_string(texel.Normal[1]) + ", " +
                std::to_string(texel.Normal[2]) + ") and length squared " + std::to_string(length);
        return std::nullopt;
      }
    }
  }
  return views;
}

}

std::optional<Content::ImpostorAtlas> ImpostorBaker::Bake(ImpostorCapture capture,
                                                          Content::ImpostorAtlasShape shape,
                                                          std::string &error) {
  if (shape.Pixels < 3 || shape.Pixels > 4096 || shape.Views == 0 || shape.Views > 64 ||
      static_cast<size_t>(shape.Pixels) * static_cast<size_t>(shape.Pixels) * shape.Views >
          kMostAtlasTexels) {
    error = Says::Shape;
    return std::nullopt;
  }
  const Vec3 centre = (capture.LeastM + capture.MostM) * 0.5;
  const Vec3 half = (capture.MostM - capture.LeastM) * 0.5;
  const double halfExtent = std::max(std::hypot(half[0], half[2]), half[1]) *
                            static_cast<double>(shape.Pixels) /
                            static_cast<double>(shape.Pixels - 2);
  if (capture.Sources.empty() || !(halfExtent > 0.0) || !std::isfinite(halfExtent)) {
    error = Says::Bounds;
    return std::nullopt;
  }
  PlanSpec specification;
  specification.Outputs = {Resource::SceneHdr,
                           Resource::SceneDepth,
                           Resource::SceneShadingNormal,
                           Resource::SceneSurfaceIdentity};
  specification.Content = {Stage::Subjects};
  auto plan = Compiled::Compile(specification);
  if (!plan) {
    error = std::move(plan.error());
    return std::nullopt;
  }
  SceneRenderer renderer;
  if (auto initialized = renderer.Init({.WidthPx = shape.Pixels, .HeightPx = shape.Pixels}, *plan);
      !initialized) {
    error = std::move(initialized.error());
    return std::nullopt;
  }
  std::vector<Material> surfaces;
  for (auto &source : capture.Sources) {
    if (!InstallSource(renderer, std::move(source), surfaces, error)) { return std::nullopt; }
  }
  const Vec3 toSun = EastUpSouthDirection(kCaptureLightBearingDeg * kDeg2Rad,
                                          kCaptureLightElevationDeg * kDeg2Rad);
  SubjectLight key;
  key.Light.Kind = LightKind::Directional;
  key.Light.Intensity = static_cast<float>(kCaptureIlluminanceLux);
  for (int axis = 0; axis < 3; ++axis) {
    key.Light.Direction[axis] = static_cast<float>(-toSun[axis]);
  }
  if (!renderer.SetSubjectLights(std::span(&key, 1), error)) { return std::nullopt; }
  renderer.SetSubjectEnvironment({});

  auto views = CaptureViews(renderer, centre, halfExtent, shape, surfaces.size(), error);
  if (!views) { return std::nullopt; }
  return Content::ImpostorAtlas::Create(
      shape.Pixels, centre, halfExtent, std::move(surfaces), std::move(*views), error);
}

}
