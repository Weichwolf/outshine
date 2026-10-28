#include "ImpostorBaker.h"
#include "ImpostorCaptureMaterial.h"
#include "ImpostorViewFrame.h"

#include "ExecutionPlan.h"
#include "Lens.h"
#include "SceneRenderer.h"
#include "RenderGeometryStorage.h"
#include "StoredVertex.h"
#include "MaterialBinding.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Render {
namespace {
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

struct NativePiece {
  std::vector<StoredVertex> Vertices;
  std::vector<uint32_t> Indices;
  std::vector<float> Tangents;
  std::vector<float> Colours;
  std::vector<float> Uv1;
};

struct CapturePieces {
  SceneRenderer &Renderer;
  std::vector<PieceHandle> Handles;
  std::vector<uint32_t> Materials;

  ~CapturePieces() {
    for (const auto handle : Handles) { Renderer.ReleasePiece(handle); }
  }
};

std::optional<ImpostorAtlas::View> ConvertReadback(const SurfaceReadback &readback,
                                                   std::span<const uint32_t> materials,
                                                   std::string &error) {
  const size_t count = readback.Pixels();
  ImpostorAtlas::View view;
  view.Texels.resize(count);
  view.Materials.resize(count);
  for (size_t pixel = 0; pixel < count; ++pixel) {
    const auto sample = readback.Read(pixel);
    const float surface = sample.Identity;
    if (!std::isfinite(surface) || surface < 0 ||
        static_cast<double>(surface) > static_cast<double>(materials.size()) ||
        surface != std::floor(surface)) {
      error = Says::Surface;
      return std::nullopt;
    }
    const uint32_t local = surface == 0 ? 0 : materials[static_cast<uint32_t>(surface) - 1];
    if (surface > 0 && local == 0) {
      error = Says::Surface;
      return std::nullopt;
    }
    view.Texels[pixel] = {.Normal = sample.Normal, .Depth = sample.Depth, .Surface = local};
    view.Materials[pixel] = {
        .BaseColour = sample.Base, .Roughness = sample.Roughness, .Metalness = sample.Metalness};
  }
  return view;
}

std::optional<NativePiece> BuildPiece(const RenderGeometryPart &part,
                                      std::span<const uint32_t> indices,
                                      const Material &material,
                                      std::string &error) {
  if (part.PositionsM.size() != part.VertexCount * 3 ||
      part.Normals.size() != part.VertexCount * 3 ||
      (!part.Uv.empty() && part.Uv.size() != part.VertexCount * 2) ||
      (material.Pattern == SurfacePattern::FacadeMetres &&
       (part.Uv.size() != part.VertexCount * 2 || part.Uv1.size() != part.VertexCount * 2)) ||
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
  if (material.NormalMap.bound()) {
    piece.Tangents.assign(part.Tangents.begin(), part.Tangents.end());
  }
  piece.Colours.assign(part.Colours.begin(), part.Colours.end());
  if (material.Pattern == SurfacePattern::FacadeMetres) {
    piece.Uv1.assign(part.Uv1.begin(), part.Uv1.end());
  }
  return piece;
}

bool InstallSource(CapturePieces &pieces,
                   ImpostorCaptureSource source,
                   std::vector<Material> &surfaces,
                   std::string &error) {
  auto &renderer = pieces.Renderer;
  if (!source.Mesh.wellFormed() || source.Mesh.parts() == 0 || source.Mesh.surfaces() == 0 ||
      source.Instances.empty() || source.Instances.size() > std::numeric_limits<uint32_t>::max()) {
    error = Says::Geometry;
    return false;
  }
  RenderGeometryStorage storage;
  const auto shaped = PrepareRenderGeometry(source.Mesh, storage);
  if (!shaped) {
    error = Describe(shaped.error());
    return false;
  }
  const size_t firstAtlasSurface = surfaces.size();
  for (int at = 0; at < source.Mesh.surfaces(); ++at) {
    auto material = CapturedImpostorMaterial(source.Mesh.surfaceAt(MaterialInstance(at)));
    if (!material) {
      error = "impostor capture cannot preserve this material; retain native geometry";
      return false;
    }
    surfaces.push_back(*material);
  }
  auto firstSurface = renderer.RegisterPieceMaterials(std::move(source.Mesh));
  if (!firstSurface) {
    error = std::move(firstSurface).error();
    return false;
  }
  for (size_t at = 0; at < shaped->Surfaces.size(); ++at) {
    const uint32_t row = firstSurface->At(static_cast<uint32_t>(at));
    if (row == kNoMaterialSlot || firstAtlasSurface + at >= std::numeric_limits<uint32_t>::max()) {
      error = Says::Surface;
      return false;
    }
    if (row >= pieces.Materials.size()) { pieces.Materials.resize(size_t{row} + 1); }
    pieces.Materials[row] = static_cast<uint32_t>(firstAtlasSurface + at + 1);
  }
  for (const RenderGeometryPart &part : shaped->Parts) {
    if (part.Material < 0 || static_cast<size_t>(part.Material) >= shaped->Surfaces.size()) {
      error = Says::Geometry;
      return false;
    }
    const Material &material = shaped->Surfaces[static_cast<size_t>(part.Material)];
    auto native = BuildPiece(part, shaped->Indices, material, error);
    if (!native) { return false; }
    PieceMesh piece;
    piece.Verts = native->Vertices;
    piece.Indices = native->Indices;
    piece.Tangents = native->Tangents;
    piece.Colours = native->Colours;
    piece.Uv1 = native->Uv1;
    piece.Instances = source.Instances;
    piece.MaxInstances = static_cast<uint32_t>(source.Instances.size());
    piece.Surface = PieceSurface::Registered(*firstSurface, static_cast<uint32_t>(part.Material));
    piece.Textured = !part.Uv.empty();
    auto placed = renderer.PlacePiece(piece);
    if (!placed) {
      error = std::move(placed.error());
      return false;
    }
    pieces.Handles.push_back(*placed);
  }
  return true;
}

std::optional<Viewpoint>
MakeCaptureView(const Vec3 &centre, const Vec3 &direction, double halfExtent, bool ring) {
  auto camera =
      Viewpoint::LookAt({.EyeM = centre + direction * (3 * halfExtent), .AimM = centre}, 0);
  if (!ring) {
    const auto frame = ImpostorViewFrame::From(direction);
    if (!frame) { return std::nullopt; }
    camera = Viewpoint{};
    camera->EyeM = centre + direction * (3 * halfExtent);
    camera->Forward = direction * -1.0;
    camera->Right = frame->Right;
    camera->Up = frame->Up;
  }
  if (!camera) { return std::nullopt; }
  camera->Kind = CameraKind::Orthographic;
  camera->XMagM = camera->YMagM = halfExtent;
  camera->ZNearM = halfExtent;
  camera->ZFarM = 5 * halfExtent;
  return camera;
}

std::optional<std::vector<ImpostorAtlas::View>> CaptureViews(SceneRenderer &renderer,
                                                             const Vec3 &centre,
                                                             double halfExtent,
                                                             ImpostorAtlasShape shape,
                                                             std::span<const uint32_t> materials,
                                                             std::span<const Vec3> directions,
                                                             std::string &error) {
  std::vector<ImpostorAtlas::View> views;
  views.reserve(shape.Views);
  SurfaceReadback surface;
  for (unsigned at = 0; at < shape.Views; ++at) {
    const double angle = 2 * std::numbers::pi * at / shape.Views;
    const Vec3 direction =
        directions.empty() ? Vec3{{std::sin(angle), 0, std::cos(angle)}} : directions[at];
    const auto camera = MakeCaptureView(centre, direction, halfExtent, directions.empty());
    if (!camera) {
      error = Says::Bounds;
      return std::nullopt;
    }
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
    if (renderer.ReadSurface(surface) != ReadState::Ready) {
      error = Says::Readback;
      return std::nullopt;
    }
    auto view = ConvertReadback(surface, materials, error);
    if (!view) { return std::nullopt; }
    view->TowardEye = direction;
    views.push_back(std::move(*view));
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

std::optional<ImpostorCaptureProjection> ImpostorBaker::ProjectionFor(
    const ImpostorCapture &capture, ImpostorAtlasShape shape, std::string &error) {
  error.clear();
  if (shape.Pixels < 3 || shape.Pixels > 4096 || shape.Views == 0 || shape.Views > 64 ||
      static_cast<size_t>(shape.Pixels) * static_cast<size_t>(shape.Pixels) * shape.Views >
          kMostAtlasTexels) {
    error = Says::Shape;
    return std::nullopt;
  }
  const Vec3 centre = (capture.LeastM + capture.MostM) * 0.5;
  const Vec3 half = (capture.MostM - capture.LeastM) * 0.5;
  if (!std::ranges::all_of(half, [](double value) { return std::isfinite(value) && value >= 0; }) ||
      !std::ranges::all_of(centre, [](double value) { return std::isfinite(value); })) {
    error = Says::Bounds;
    return std::nullopt;
  }
  double radius = std::max(std::hypot(half[0], half[2]), half[1]);
  if (!capture.ViewDirections.empty()) {
    if (capture.ViewDirections.size() != shape.Views) {
      error = Says::Shape;
      return std::nullopt;
    }
    radius = 0;
    for (const auto &direction : capture.ViewDirections) {
      const auto frame = ImpostorViewFrame::From(direction);
      if (!frame) {
        error = Says::Bounds;
        return std::nullopt;
      }
      const auto projected = [&](const Vec3 &axis) {
        return std::abs(axis[0]) * half[0] + std::abs(axis[1]) * half[1] +
               std::abs(axis[2]) * half[2];
      };
      radius = std::max(
          {radius, projected(frame->Right), projected(frame->Up), projected(direction) * 0.5});
    }
  }
  const double halfExtent =
      radius * static_cast<double>(shape.Pixels) / static_cast<double>(shape.Pixels - 2);
  if (!(halfExtent > 0.0) || !std::isfinite(halfExtent)) {
    error = Says::Bounds;
    return std::nullopt;
  }
  const double requested = capture.HalfExtentM.value_or(halfExtent);
  if (!std::isfinite(requested) || requested < halfExtent) {
    error = Says::Bounds;
    return std::nullopt;
  }
  return ImpostorCaptureProjection{.Centre = centre, .HalfExtentM = requested};
}

std::optional<ImpostorAtlas>
ImpostorBaker::Capture(ImpostorCapture capture, ImpostorAtlasShape shape, std::string &error) {
  const auto projection = ProjectionFor(capture, shape, error);
  if (!projection) { return std::nullopt; }
  if (capture.Sources.empty()) {
    error = Says::Geometry;
    return std::nullopt;
  }
  const auto &centre = projection->Centre;
  const double halfExtent = projection->HalfExtentM;
  if (!Prepare(shape.Pixels, error)) { return std::nullopt; }
  auto &renderer = *Renderer_;
  const auto offline = renderer.OfflineUploads(std::chrono::seconds(5));
  CapturePieces pieces{.Renderer = renderer, .Handles = {}, .Materials = {}};
  std::vector<Material> surfaces;
  for (auto &source : capture.Sources) {
    if (!InstallSource(pieces, std::move(source), surfaces, error)) { return std::nullopt; }
  }
  auto views = CaptureViews(
      renderer, centre, halfExtent, shape, pieces.Materials, capture.ViewDirections, error);
  if (!views) { return std::nullopt; }
  return ImpostorAtlas::Create(
      shape.Pixels, centre, halfExtent, std::move(surfaces), std::move(*views), error);
}

ImpostorBaker::ImpostorBaker(GpuSubmission submission)
    : Renderer_(std::make_unique<SceneRenderer>(submission)) {}

ImpostorBaker::~ImpostorBaker() = default;

bool ImpostorBaker::Prepare(int pixels, std::string &error) {
  if (Pixels_ == pixels && Renderer_->DeviceUsable()) { return true; }
  if (!Plan_) {
    PlanSpec specification;
    specification.Outputs = {Resource::SceneHdr,
                             Resource::SceneDepth,
                             Resource::SceneShadingNormal,
                             Resource::SceneSurfaceIdentity,
                             Resource::SceneSurfaceBase,
                             Resource::SceneSurfaceMetalRough};
    specification.Content = {Stage::OpaqueGeometry};
    auto plan = ExecutionPlan::Compile(specification);
    if (!plan) {
      error = std::move(plan.error());
      return false;
    }
    Plan_ = std::move(*plan);
  }
  if (auto initialized = Renderer_->Init({.WidthPx = pixels, .HeightPx = pixels}, Plan_);
      !initialized) {
    Pixels_ = 0;
    error = std::move(initialized.error());
    return false;
  }
  Pixels_ = pixels;
  return true;
}

std::optional<ImpostorAtlas>
ImpostorBaker::Bake(ImpostorCapture capture, ImpostorAtlasShape shape, std::string &error) {
  ImpostorBaker baker;
  return baker.Capture(std::move(capture), shape, error);
}

}
