#include "ImpostorAtlas.h"

#include "Digest.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace outshine::Content {
namespace {

constexpr uint64_t kAtlasMagic = 0x004e574f5243534full;
constexpr uint32_t kAtlasVersion = 1;
constexpr size_t kAtlasHeaderBytes =
    2 * sizeof(uint64_t) + 4 * sizeof(uint32_t) + 4 * sizeof(double);
constexpr size_t kAtlasMaterialBytes = 10 * sizeof(float) + 3 * sizeof(uint32_t);
constexpr size_t kAtlasViewBytes = 3 * sizeof(double);
constexpr size_t kAtlasTexelBytes = 4 * sizeof(float) + sizeof(uint32_t);
constexpr size_t kAtlasChecksumBytes = sizeof(uint64_t);
constexpr size_t kMostAtlasTexels = 1u << 24u;
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

struct AtlasWriter {
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

struct AtlasReader {
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

bool CacheableView(const ImpostorAtlas::View &view, const ImpostorAtlas &atlas) {
  const auto texels = static_cast<size_t>(atlas.Pixels()) * static_cast<size_t>(atlas.Pixels());
  const double norm = Dot(view.TowardEye, view.TowardEye);
  if (!std::isfinite(norm) || std::abs(norm - 1) > kUnitDirectionSquaredTolerance ||
      view.TowardEye[1] != 0 || view.Texels.size() != texels) {
    return false;
  }
  return std::ranges::all_of(view.Texels, [&](const ImpostorAtlas::Texel &pixel) {
    const float normal = Dot(pixel.Normal, pixel.Normal);
    return std::isfinite(pixel.Depth) && pixel.Depth >= 0 && pixel.Depth <= 1 &&
           pixel.Surface <= atlas.Surfaces().size() && ((pixel.Surface > 0) == (pixel.Depth > 0)) &&
           std::isfinite(normal) &&
           (pixel.Surface > 0 ? std::abs(normal - 1) <= kCapturedNormalSquaredTolerance
                              : normal == 0);
  });
}

bool Cacheable(const ImpostorAtlas &atlas) {
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
         std::ranges::all_of(atlas.Views(), [&](const ImpostorAtlas::View &view) {
           return CacheableView(view, atlas);
         });
}

}

std::optional<ImpostorAtlas> ImpostorAtlas::Create(int pixels,
                                                   Vec3 centreM,
                                                   double halfExtentM,
                                                   std::vector<Material> surfaces,
                                                   std::vector<View> views,
                                                   std::string &error) {
  ImpostorAtlas atlas;
  atlas.Pixels_ = pixels;
  atlas.CentreM_ = centreM;
  atlas.HalfExtentM_ = halfExtentM;
  atlas.Surfaces_ = std::move(surfaces);
  atlas.Views_ = std::move(views);
  if (!Cacheable(atlas)) {
    error = "impostor atlas contains unsupported or invalid samples";
    return std::nullopt;
  }
  return atlas;
}

std::optional<std::vector<uint8_t>> ImpostorAtlas::Encode(std::string_view provenance,
                                                          std::string &error) const {
  if (provenance.empty() || !Cacheable(*this)) {
    error = "impostor artifact requires provenance, valid samples and supported materials";
    return std::nullopt;
  }
  AtlasWriter out;
  out.Bytes.reserve(kAtlasHeaderBytes + Surfaces_.size() * kAtlasMaterialBytes +
                    Views_.size() *
                        (kAtlasViewBytes + static_cast<size_t>(Pixels_) *
                                               static_cast<size_t>(Pixels_) * kAtlasTexelBytes) +
                    kAtlasChecksumBytes);
  out.Put(kAtlasMagic);
  out.Put(kAtlasVersion);
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

std::optional<ImpostorAtlas> ImpostorAtlas::Decode(std::span<const uint8_t> bytes,
                                                   std::string_view provenance,
                                                   std::string &error) {
  const auto refuse = [&] -> std::optional<ImpostorAtlas> {
    error = "impostor artifact is stale, corrupt, unsupported or outside its bounds";
    return std::nullopt;
  };
  if (provenance.empty() || bytes.size() < kAtlasHeaderBytes + kAtlasChecksumBytes) {
    return refuse();
  }
  AtlasReader checksum{.Bytes = bytes.last(kAtlasChecksumBytes)};
  if (Hash(bytes.first(bytes.size() - kAtlasChecksumBytes)) != checksum.Take<uint64_t>()) {
    return refuse();
  }
  AtlasReader in{.Bytes = bytes};
  if (in.Take<uint64_t>() != kAtlasMagic || in.Take<uint32_t>() != kAtlasVersion ||
      in.Take<uint64_t>() != Provenance(provenance)) {
    return refuse();
  }
  const auto pixels = in.Take<uint32_t>();
  const auto views = in.Take<uint32_t>();
  const auto surfaces = in.Take<uint32_t>();
  const auto count = static_cast<uint64_t>(pixels) * pixels;
  if (pixels < 3 || pixels > 4096 || views == 0 || views > 64 || surfaces == 0 || surfaces > 64 ||
      count * views > kMostAtlasTexels ||
      kAtlasHeaderBytes + static_cast<uint64_t>(surfaces) * kAtlasMaterialBytes +
              static_cast<uint64_t>(views) * (kAtlasViewBytes + count * kAtlasTexelBytes) +
              kAtlasChecksumBytes !=
          bytes.size()) {
    return refuse();
  }
  ImpostorAtlas atlas;
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

}
