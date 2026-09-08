#include "CrownAtlas.h"
#include "../../build/CrownBuild.h"
#include "Digest.h"
#include <bit>
#include <type_traits>
#include <Outshine.h>
#include <scenario/Scenario.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <array>

namespace outshine {
namespace {
constexpr size_t kMostAtlasTexels = 1u << 24u;

namespace Says {
constexpr auto Shape =
    "crown atlas requires 3..4096 pixels, 1..64 views and at most 16777216 texels";
constexpr auto Geometry = "crown atlas requires native tree geometry";
constexpr auto Bounds = "crown atlas requires finite nonempty bounds";
constexpr auto Readback = "crown atlas readback has incomplete attachments";
constexpr auto Surface = "crown atlas material identity is outside its source table";
}

uint8_t Byte(float value) {
  return static_cast<uint8_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 255.0f));
}

float Srgb(float linear) {
  return linear <= 0.0031308f ? 12.92f * linear : 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;
}
}

namespace {
constexpr uint64_t kCrownMagic = 0x004e574f5243534full;
constexpr uint32_t kCrownVersion = 1;
constexpr size_t kCrownHeaderBytes = 64;
constexpr size_t kCrownMaterialBytes = 52;

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
    using Word = std::conditional_t<sizeof(T) == 4, uint32_t, uint64_t>;
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
    using Word = std::conditional_t<sizeof(T) == 4, uint32_t, uint64_t>;
    Word bits = 0;
    for (size_t at = 0; at < sizeof(T); ++at) { bits |= Word(Bytes[At++]) << (at * 8); }
    return std::bit_cast<T>(bits);
  }
};

bool Cacheable(const CrownAtlas &atlas) {
  if (atlas.Pixels() < 3 || atlas.Pixels() > 4096 || atlas.Views().empty() ||
      atlas.Views().size() > 64 || atlas.Surfaces().empty() || atlas.Surfaces().size() > 64 ||
      size_t(atlas.Pixels()) * size_t(atlas.Pixels()) * atlas.Views().size() > kMostAtlasTexels ||
      !std::isfinite(atlas.HalfExtentM()) || atlas.HalfExtentM() <= 0) {
    return false;
  }
  for (const auto x : atlas.CentreM()) {
    if (!std::isfinite(x)) { return false; }
  }
  for (const auto &source : atlas.Surfaces()) {
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
    for (const float x : core.BaseColour) {
      if (!std::isfinite(x) || x < 0 || x > 1) { return false; }
    }
    for (const float x : core.Emission) {
      if (!std::isfinite(x) || x < 0) { return false; }
    }
    for (const float x : {core.Metalness, core.Roughness, core.CoverageCut}) {
      if (!std::isfinite(x) || x < 0 || x > 1) { return false; }
    }
  }
  for (const auto &view : atlas.Views()) {
    const double norm = Dot(view.TowardEye, view.TowardEye);
    if (!std::isfinite(norm) || std::abs(norm - 1) > 1e-12 || view.TowardEye[1] != 0 ||
        view.Texels.size() != size_t(atlas.Pixels()) * size_t(atlas.Pixels())) {
      return false;
    }
    for (const auto &pixel : view.Texels) {
      const float normal = Dot(pixel.Normal, pixel.Normal);
      if (!std::isfinite(pixel.Depth) || pixel.Depth < 0 || pixel.Depth > 1 ||
          pixel.Surface > atlas.Surfaces().size() || ((pixel.Surface > 0) != (pixel.Depth > 0)) ||
          !std::isfinite(normal) ||
          (pixel.Surface > 0 ? std::abs(normal - 1) > .003f : normal != 0)) {
        return false;
      }
    }
  }
  return true;
}
}

std::string CrownAtlas::ProvenanceFor(std::string_view species, Shape shape) {
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
                    Views_.size() * (24 + size_t(Pixels_) * size_t(Pixels_) * 20) + 8);
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
  const auto refuse = [&]() -> std::optional<CrownAtlas> {
    error = "crown artifact is stale, corrupt, unsupported or outside its bounds";
    return std::nullopt;
  };
  if (provenance.empty() || bytes.size() < kCrownHeaderBytes + 8) { return refuse(); }
  CrownReader checksum{bytes.last(8)};
  if (Hash(bytes.first(bytes.size() - 8)) != checksum.Take<uint64_t>()) { return refuse(); }
  CrownReader in{bytes};
  if (in.Take<uint64_t>() != kCrownMagic || in.Take<uint32_t>() != kCrownVersion ||
      in.Take<uint64_t>() != Provenance(provenance)) {
    return refuse();
  }
  const uint32_t pixels = in.Take<uint32_t>(), views = in.Take<uint32_t>(),
                 surfaces = in.Take<uint32_t>();
  const uint64_t count = uint64_t(pixels) * pixels;
  if (pixels < 3 || pixels > 4096 || views == 0 || views > 64 || surfaces == 0 || surfaces > 64 ||
      count * views > kMostAtlasTexels ||
      kCrownHeaderBytes + uint64_t(surfaces) * kCrownMaterialBytes +
              uint64_t(views) * (24 + count * 20) + 8 !=
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
    const uint32_t alpha = in.Take<uint32_t>(), sided = in.Take<uint32_t>(),
                   unlit = in.Take<uint32_t>();
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

std::optional<Geometry> CrownAtlas::GeometryAt(size_t view) const {
  if (view >= Views_.size()) { return std::nullopt; }
  const View &source = Views_[view];
  const size_t count = source.Texels.size();
  constexpr uint32_t missing = std::numeric_limits<uint32_t>::max();
  std::vector<uint32_t> owner(count, missing), queue;
  queue.reserve(count);
  for (uint32_t at = 0; at < count; ++at) {
    if (source.Texels[at].Surface == 0) { continue; }
    owner[at] = at;
    queue.push_back(at);
  }
  if (queue.empty()) { return std::nullopt; }
  const auto width = static_cast<uint32_t>(Pixels_);
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
  const Vec3 toward = source.TowardEye;
  const Vec3 right{{toward[2], 0, -toward[0]}};
  std::array<std::vector<uint8_t>, 3> images;
  for (auto &image : images) { image.resize(count * 4); }
  for (size_t at = 0; at < count; ++at) {
    const Texel &pixel = source.Texels[owner[at]];
    const Material &material = Surfaces_[pixel.Surface - 1];
    Vec3 n{{pixel.Normal[0], pixel.Normal[1], pixel.Normal[2]}};
    if (!Normalise(n)) { return std::nullopt; }
    for (size_t c = 0; c < 3; ++c) { images[0][at * 4 + c] = Byte(Srgb(material.BaseColour[c])); }
    images[0][at * 4 + 3] = source.Texels[at].Surface > 0 ? 255 : 0;
    images[1][at * 4] = Byte(static_cast<float>(0.5 * (Dot(n, right) + 1.0)));
    images[1][at * 4 + 1] = Byte(static_cast<float>(0.5 * (1.0 - n[1])));
    images[1][at * 4 + 2] = Byte(static_cast<float>(0.5 * (Dot(n, toward) + 1.0)));
    images[1][at * 4 + 3] = 255;
    images[2][at * 4] = 255;
    images[2][at * 4 + 1] = Byte(material.Roughness);
    images[2][at * 4 + 2] = Byte(material.Metalness);
    images[2][at * 4 + 3] = 255;
  }
  Geometry geometry;
  Material material;
  material.BaseColour = {{1, 1, 1, 1}};
  material.Metalness = material.Roughness = 1;
  material.Alpha = AlphaMode::Masked;
  std::array<SurfaceMap *, 3> maps{
      &material.BaseColourMap, &material.NormalMap, &material.MetalRoughMap};
  for (size_t at = 0; at < maps.size(); ++at) {
    maps[at]->Image = geometry.addImage(Pixels_, Pixels_, images[at]);
    if (maps[at]->Image < 0) { return std::nullopt; }
    maps[at]->Sampler.WrapU = maps[at]->Sampler.WrapV = Wrap::ClampToEdge;
  }
  const int part = geometry.addPart("crown", geometry.addSurface("crown", material));
  const std::array<Vec3, 4> corners{CentreM_ - right * HalfExtentM_ - Vec3{{0, HalfExtentM_, 0}},
                                    CentreM_ + right * HalfExtentM_ - Vec3{{0, HalfExtentM_, 0}},
                                    CentreM_ + right * HalfExtentM_ + Vec3{{0, HalfExtentM_, 0}},
                                    CentreM_ - right * HalfExtentM_ + Vec3{{0, HalfExtentM_, 0}}};
  std::array<float, 12> positions, normals;
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

std::optional<CrownAtlas>
CrownAtlas::Bake(const Generators::TreePrototype &tree, Shape shape, std::string &error) {
  if (shape.Pixels < 3 || shape.Pixels > 4096 || shape.Views == 0 || shape.Views > 64 ||
      static_cast<size_t>(shape.Pixels) * static_cast<size_t>(shape.Pixels) * shape.Views >
          kMostAtlasTexels) {
    error = Says::Shape;
    return std::nullopt;
  }
  auto geometry = tree.GeometryAt(0);
  if (!geometry) {
    error = Says::Geometry;
    return std::nullopt;
  }
  CrownAtlas atlas;
  atlas.Pixels_ = shape.Pixels;
  Vec3 least{{std::numeric_limits<double>::infinity(),
              std::numeric_limits<double>::infinity(),
              std::numeric_limits<double>::infinity()}};
  Vec3 most = least * -1.0;
  for (int part = 0; part < geometry->parts(); ++part) {
    const auto positions = geometry->positionsOf(part);
    for (size_t at = 0; at < positions.size(); at += 3) {
      for (size_t axis = 0; axis < 3; ++axis) {
        least[axis] = std::min(least[axis], static_cast<double>(positions[at + axis]));
        most[axis] = std::max(most[axis], static_cast<double>(positions[at + axis]));
      }
    }
  }
  atlas.CentreM_ = (least + most) * 0.5;
  const Vec3 half = (most - least) * 0.5;
  atlas.HalfExtentM_ = std::max(std::hypot(half[0], half[2]), half[1]) *
                       static_cast<double>(shape.Pixels) / static_cast<double>(shape.Pixels - 2);
  if (!(atlas.HalfExtentM_ > 0.0) || !std::isfinite(atlas.HalfExtentM_)) {
    error = Says::Bounds;
    return std::nullopt;
  }
  for (int at = 0; at < geometry->surfaces(); ++at) {
    atlas.Surfaces_.push_back(geometry->surfaceAt(MaterialInstance(at)));
  }
  Scenario::Document scenario;
  scenario.Render.Declared = true;
  scenario.Render.Frame = {shape.Pixels, shape.Pixels};
  scenario.Render.Outputs = {"sceneDepth", "sceneShadingNormal", "sceneSurfaceIdentity"};
  scenario.Lit.Declared = true;
  scenario.Lit.Key.Lux = 20000;
  scenario.Lit.Key.BearingDeg = 135;
  scenario.Lit.Key.ElevationDeg = 40;
  for (unsigned at = 0; at < shape.Views; ++at) {
    const double angle = 2.0 * std::numbers::pi * at / shape.Views;
    const Vec3 direction{{std::sin(angle), 0, std::cos(angle)}};
    atlas.Views_.push_back({.TowardEye = direction});
    Scenario::View view;
    view.Id = std::to_string(at);
    view.Person = "first";
    view.Sees.Placed = true;
    view.Sees.Stands.AtM = atlas.CentreM_ + direction * (3.0 * atlas.HalfExtentM_);
    view.Sees.LooksAt = true;
    view.Sees.LookAtM = atlas.CentreM_;
    view.Sees.setProjection(Scenario::Camera::Ortho{.XMagM = atlas.HalfExtentM_,
                                                    .YMagM = atlas.HalfExtentM_,
                                                    .NearM = atlas.HalfExtentM_,
                                                    .FarM = 5.0 * atlas.HalfExtentM_});
    scenario.Views.push_back(view);
  }
  std::vector<float> depth, normal, identity;
  const size_t count = static_cast<size_t>(shape.Pixels) * static_cast<size_t>(shape.Pixels);
  for (unsigned at = 0; at < shape.Views; ++at) {
    Engine engine;
    if (!engine.drawsInto({shape.Pixels, shape.Pixels}) || !engine.declare(scenario) ||
        !engine.setView(std::to_string(at)) || !engine.setGeometry(*geometry) ||
        !engine.assemble() || !engine.advance()) {
      error = engine.error();
      return std::nullopt;
    }
    if (!engine.renderer().render({}) || !engine.renderer().readPixels(Buffer::Depth, depth) ||
        !engine.renderer().readPixels(Buffer::ShadingNormal, normal) ||
        !engine.renderer().readPixels(Buffer::SurfaceIdentity, identity)) {
      error = engine.error();
      return std::nullopt;
    }
    if (depth.size() != count || normal.size() != count * 4 || identity.size() != count * 4) {
      error = Says::Readback;
      return std::nullopt;
    }
    auto &texels = atlas.Views_[at].Texels;
    texels.resize(count);
    for (size_t pixel = 0; pixel < count; ++pixel) {
      const float surface = identity[pixel * 4];
      if (!std::isfinite(surface) || surface < 0 || surface > atlas.Surfaces_.size() ||
          surface != std::floor(surface)) {
        error = Says::Surface;
        return std::nullopt;
      }
      texels[pixel] = {
          .Normal = {{normal[pixel * 4], normal[pixel * 4 + 1], normal[pixel * 4 + 2]}},
          .Depth = depth[pixel],
          .Surface = static_cast<uint32_t>(surface)};
    }
  }
  return atlas;
}

}
