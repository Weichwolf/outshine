#include "math/Srgb.h"
#include "CrownAtlas.h"
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

namespace {
constexpr uint64_t kCrownMagic = 0x004e574f5243534full;
constexpr uint32_t kCrownVersion = 1;
constexpr size_t kCrownHeaderBytes =
    2 * sizeof(uint64_t) + 4 * sizeof(uint32_t) + 4 * sizeof(double);
constexpr size_t kCrownMaterialBytes = 10 * sizeof(float) + 3 * sizeof(uint32_t);
constexpr size_t kCrownViewBytes = 3 * sizeof(double);
constexpr size_t kCrownTexelBytes = 4 * sizeof(float) + sizeof(uint32_t);
constexpr size_t kCrownChecksumBytes = sizeof(uint64_t);
constexpr double kUnitDirectionSquaredTolerance = 1e-12;
constexpr float kCapturedNormalSquaredTolerance = 0.003f;
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559);

uint64_t Hash(std::span<const uint8_t> bytes) {
  uint64_t value = kDigestBasis;
  for (const auto byte : bytes) { value = DigestFolded(value, byte); }
  return value;
}

uint64_t Provenance(std::string_view value) {
  return Hash({reinterpret_cast<const uint8_t *>(value.data()), value.size()});
}

struct CrownWriter {
  std::vector<uint8_t> Bytes;

  template <typename T> void Put(T value) {
    static_assert(sizeof(T) == sizeof(uint32_t) || sizeof(T) == sizeof(uint64_t));
    using Word = std::conditional_t<sizeof(T) == sizeof(uint32_t), uint32_t, uint64_t>;
    const Word bits = std::bit_cast<Word>(value);
    for (size_t at = 0; at < sizeof(T); ++at) {
      Bytes.push_back(static_cast<uint8_t>(bits >> (at * 8)));
    }
  }
};

struct CrownReader {
  std::span<const uint8_t> Bytes;
  size_t At = 0;

  template <typename T> T Take() {
    static_assert(sizeof(T) == sizeof(uint32_t) || sizeof(T) == sizeof(uint64_t));
    using Word = std::conditional_t<sizeof(T) == sizeof(uint32_t), uint32_t, uint64_t>;
    Word bits = 0;
    for (size_t at = 0; at < sizeof(T); ++at) {
      bits |= static_cast<Word>(Bytes[At++]) << (at * 8);
    }
    return std::bit_cast<T>(bits);
  }
};

bool CacheableMaterial(const Material &source) {
  Material core;
  core.BaseColour = source.BaseColour;
  core.Metalness = source.Metalness;
  core.Roughness = source.Roughness;
  core.Emission = source.Emission;
  core.Alpha = source.Alpha;
  core.CoverageCut = source.CoverageCut;
  core.DoubleSided = source.DoubleSided;
  core.Unlit = source.Unlit;
  if (!(source == core) || static_cast<uint32_t>(core.Alpha) > 2) { return false; }
  const auto unitFactor = [](float x) { return std::isfinite(x) && x >= 0 && x <= 1; };
  const std::array factors{core.Metalness, core.Roughness, core.CoverageCut};
  return std::ranges::all_of(core.BaseColour, unitFactor) &&
         std::ranges::all_of(core.Emission, [](float x) { return std::isfinite(x) && x >= 0; }) &&
         std::ranges::all_of(factors, unitFactor);
}

bool CacheableView(const CrownAtlas::View &view, const CrownAtlas &atlas) {
  const auto texels = static_cast<size_t>(atlas.Pixels()) * static_cast<size_t>(atlas.Pixels());
  const double norm = Dot(view.TowardEye, view.TowardEye);
  if (!std::isfinite(norm) || std::abs(norm - 1) > kUnitDirectionSquaredTolerance ||
      view.TowardEye[1] != 0 || view.Texels.size() != texels) {
    return false;
  }
  return std::ranges::all_of(view.Texels, [&](const CrownAtlas::Texel &pixel) {
    const float normal = Dot(pixel.Normal, pixel.Normal);
    return std::isfinite(pixel.Depth) && pixel.Depth >= 0 && pixel.Depth <= 1 &&
           pixel.Surface <= atlas.Surfaces().size() && ((pixel.Surface > 0) == (pixel.Depth > 0)) &&
           std::isfinite(normal) &&
           (pixel.Surface > 0 ? std::abs(normal - 1) <= kCapturedNormalSquaredTolerance
                              : normal == 0);
  });
}

bool Cacheable(const CrownAtlas &atlas) {
  if (atlas.Pixels() < 3 || atlas.Pixels() > 4096 || atlas.Views().empty() ||
      atlas.Views().size() > 64 || atlas.Surfaces().empty() || atlas.Surfaces().size() > 64 ||
      static_cast<size_t>(atlas.Pixels()) * static_cast<size_t>(atlas.Pixels()) *
              atlas.Views().size() >
          kMostAtlasTexels ||
      !std::isfinite(atlas.HalfExtentM()) || atlas.HalfExtentM() <= 0) {
    return false;
  }
  for (const auto x : atlas.CentreM()) {
    if (!std::isfinite(x)) { return false; }
  }
  return std::ranges::all_of(atlas.Surfaces(), CacheableMaterial) &&
         std::ranges::all_of(atlas.Views(), [&](const CrownAtlas::View &view) {
           return CacheableView(view, atlas);
         });
}
}

std::string CrownAtlas::ProvenanceFor(std::string_view species, Content::ImpostorAtlasShape shape) {
  return std::string(kCrownBuildIdentity) + "/" + std::to_string(shape.Pixels) + "/" +
         std::to_string(shape.Views) + "/" + std::string(species);
}

std::optional<std::vector<uint8_t>> CrownAtlas::Encode(std::string_view provenance,
                                                       std::string &error) const {
  if (provenance.empty() || !Cacheable(*this)) {
    error = "crown artifact requires provenance, valid samples and losslessly supported materials";
    return std::nullopt;
  }
  CrownWriter out;
  out.Bytes.reserve(kCrownHeaderBytes + Surfaces_.size() * kCrownMaterialBytes +
                    Views_.size() *
                        (kCrownViewBytes + static_cast<size_t>(Pixels_) *
                                               static_cast<size_t>(Pixels_) * kCrownTexelBytes) +
                    kCrownChecksumBytes);
  out.Put(kCrownMagic);
  out.Put(kCrownVersion);
  out.Put(Provenance(provenance));
  out.Put(static_cast<uint32_t>(Pixels_));
  out.Put(static_cast<uint32_t>(Views_.size()));
  out.Put(static_cast<uint32_t>(Surfaces_.size()));
  for (const double x : CentreM_) { out.Put(x); }
  out.Put(HalfExtentM_);
  for (const auto &surface : Surfaces_) {
    for (const float x : surface.BaseColour) { out.Put(x); }
    out.Put(surface.Metalness);
    out.Put(surface.Roughness);
    for (const float x : surface.Emission) { out.Put(x); }
    out.Put(surface.CoverageCut);
    out.Put(static_cast<uint32_t>(surface.Alpha));
    out.Put(static_cast<uint32_t>(surface.DoubleSided));
    out.Put(static_cast<uint32_t>(surface.Unlit));
  }
  for (const auto &view : Views_) {
    for (const double x : view.TowardEye) { out.Put(x); }
    for (const auto &pixel : view.Texels) {
      for (const float x : pixel.Normal) { out.Put(x); }
      out.Put(pixel.Depth);
      out.Put(pixel.Surface);
    }
  }
  out.Put(Hash(out.Bytes));
  return std::move(out.Bytes);
}

std::optional<CrownAtlas> CrownAtlas::Decode(std::span<const uint8_t> bytes,
                                             std::string_view provenance,
                                             std::string &error) {
  const auto refuse = [&] -> std::optional<CrownAtlas> {
    error = "crown artifact is stale, corrupt, unsupported or outside its bounds";
    return std::nullopt;
  };
  if (provenance.empty() || bytes.size() < kCrownHeaderBytes + kCrownChecksumBytes) {
    return refuse();
  }
  CrownReader checksum{.Bytes = bytes.last(kCrownChecksumBytes)};
  if (Hash(bytes.first(bytes.size() - kCrownChecksumBytes)) != checksum.Take<uint64_t>()) {
    return refuse();
  }
  CrownReader in{.Bytes = bytes};
  if (in.Take<uint64_t>() != kCrownMagic || in.Take<uint32_t>() != kCrownVersion ||
      in.Take<uint64_t>() != Provenance(provenance)) {
    return refuse();
  }
  const auto pixels = in.Take<uint32_t>();
  const auto views = in.Take<uint32_t>();
  const auto surfaces = in.Take<uint32_t>();
  const auto count = static_cast<uint64_t>(pixels) * pixels;
  if (pixels < 3 || pixels > 4096 || views == 0 || views > 64 || surfaces == 0 || surfaces > 64 ||
      count * views > kMostAtlasTexels ||
      kCrownHeaderBytes + static_cast<uint64_t>(surfaces) * kCrownMaterialBytes +
              static_cast<uint64_t>(views) * (kCrownViewBytes + count * kCrownTexelBytes) +
              kCrownChecksumBytes !=
          bytes.size()) {
    return refuse();
  }
  CrownAtlas atlas;
  atlas.Pixels_ = static_cast<int>(pixels);
  for (double &x : atlas.CentreM_) { x = in.Take<double>(); }
  atlas.HalfExtentM_ = in.Take<double>();
  atlas.Surfaces_.resize(surfaces);
  for (auto &surface : atlas.Surfaces_) {
    for (float &x : surface.BaseColour) { x = in.Take<float>(); }
    surface.Metalness = in.Take<float>();
    surface.Roughness = in.Take<float>();
    for (float &x : surface.Emission) { x = in.Take<float>(); }
    surface.CoverageCut = in.Take<float>();
    const auto alpha = in.Take<uint32_t>();
    const auto sided = in.Take<uint32_t>();
    const auto unlit = in.Take<uint32_t>();
    if (alpha > 2 || sided > 1 || unlit > 1) { return refuse(); }
    surface.Alpha = static_cast<AlphaMode>(alpha);
    surface.DoubleSided = sided != 0;
    surface.Unlit = unlit != 0;
  }
  atlas.Views_.resize(views);
  for (auto &view : atlas.Views_) {
    for (double &x : view.TowardEye) { x = in.Take<double>(); }
    view.Texels.resize(static_cast<size_t>(count));
    for (auto &pixel : view.Texels) {
      for (float &x : pixel.Normal) { x = in.Take<float>(); }
      pixel.Depth = in.Take<float>();
      pixel.Surface = in.Take<uint32_t>();
    }
  }
  if (!Cacheable(atlas)) { return refuse(); }
  return atlas;
}

namespace {
std::vector<uint32_t> NearestCoveredTexels(const CrownAtlas::View &source, int pixels) {
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

std::optional<Geometry> CrownAtlas::GeometryAt(size_t view) const {
  if (view >= Views_.size()) { return std::nullopt; }
  const View &source = Views_[view];
  const size_t count = source.Texels.size();
  const auto owner = NearestCoveredTexels(source, Pixels_);
  if (owner.empty()) { return std::nullopt; }
  const Vec3 toward = source.TowardEye;
  const Vec3 right{{toward[2], 0, -toward[0]}};
  std::array<std::vector<uint8_t>, 3> images;
  for (auto &image : images) { image.resize(count * 4); }
  for (size_t at = 0; at < count; ++at) {
    const Texel &pixel = source.Texels[owner[at]];
    const Material &material = Surfaces_[pixel.Surface - 1];
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
    const auto image = geometry.addImage(Pixels_, Pixels_, images[at]);
    if (!image) { return std::nullopt; }
    maps[at]->Image = *image;
    maps[at]->Sampler.WrapU = maps[at]->Sampler.WrapV = Wrap::ClampToEdge;
  }
  const auto surface = geometry.addSurface("crown", material);
  if (!surface) { return std::nullopt; }
  const auto createdPart = geometry.addPart("crown", *surface);
  if (!createdPart) { return std::nullopt; }
  const int part = *createdPart;
  const std::array<Vec3, 4> corners{CentreM_ - right * HalfExtentM_ - Vec3{{0, HalfExtentM_, 0}},
                                    CentreM_ + right * HalfExtentM_ - Vec3{{0, HalfExtentM_, 0}},
                                    CentreM_ + right * HalfExtentM_ + Vec3{{0, HalfExtentM_, 0}},
                                    CentreM_ - right * HalfExtentM_ + Vec3{{0, HalfExtentM_, 0}}};
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

std::optional<std::vector<CrownAtlas::Texel>> ConvertAtlasReadback(const AtlasReadback &readback,
                                                                   std::string &error) {
  const size_t count = readback.PixelCount;
  if (readback.Depth.size() != count || readback.Normal.size() != count * 4 ||
      readback.Identity.size() != count * 4) {
    error = Says::Readback;
    return std::nullopt;
  }
  std::vector<CrownAtlas::Texel> texels(count);
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

std::optional<CrownAtlas> CrownAtlas::Bake(const Generators::TreePrototype &tree,
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
  CrownAtlas atlas;
  atlas.Pixels_ = shape.Pixels;
  const Vec3 least = geometry->LeastM;
  const Vec3 most = geometry->MostM;
  atlas.CentreM_ = (least + most) * 0.5;
  const Vec3 half = (most - least) * 0.5;
  atlas.HalfExtentM_ = std::max(std::hypot(half[0], half[2]), half[1]) *
                       static_cast<double>(shape.Pixels) / static_cast<double>(shape.Pixels - 2);
  if (!(atlas.HalfExtentM_ > 0.0) || !std::isfinite(atlas.HalfExtentM_)) {
    error = Says::Bounds;
    return std::nullopt;
  }
  const bool leaves = !geometry->Placements.empty() && geometry->Leaf.parts() > 0;
  auto prepared = PrepareAtlasGeometry(geometry->Bark, error, leaves ? &geometry->Leaf : nullptr);
  if (!prepared) { return std::nullopt; }
  atlas.Surfaces_ = std::move(prepared->Surfaces);
  std::vector<float> depth;
  std::vector<float> normal;
  std::vector<float> identity;
  const size_t count = static_cast<size_t>(shape.Pixels) * static_cast<size_t>(shape.Pixels);
  for (unsigned at = 0; at < shape.Views; ++at) {
    const double angle = 2 * std::numbers::pi * at / shape.Views;
    const Vec3 direction{{std::sin(angle), 0, std::cos(angle)}};
    atlas.Views_.push_back({.TowardEye = direction, .Texels = {}});
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
        {.EyeM = atlas.CentreM_ + direction * (3 * atlas.HalfExtentM_), .AimM = atlas.CentreM_}, 0);
    if (!camera) {
      error = Says::Bounds;
      return std::nullopt;
    }
    camera->Kind = Render::CameraKind::Orthographic;
    camera->XMagM = camera->YMagM = atlas.HalfExtentM_;
    camera->ZNearM = atlas.HalfExtentM_;
    camera->ZFarM = 5 * atlas.HalfExtentM_;
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
                                        .MaterialCount = atlas.Surfaces_.size()},
                                       error);
    if (!texels) { return std::nullopt; }
    atlas.Views_[at].Texels = std::move(*texels);
  }
  return atlas;
}

}
