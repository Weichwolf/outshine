#include "GroupImpostorView.h"
#include "ByteArchive.h"
#include "ImpostorViewFrame.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine {
namespace {

constexpr std::string_view kMagic = "OUTSHIV1";
constexpr std::string_view kCroppedMagic = "OUTSHIV2";
constexpr size_t kHeaderBytes = kMagic.size() + 2 * sizeof(uint32_t) + 3 * sizeof(double);
constexpr size_t kPatchBytes = 4 * sizeof(uint16_t) + 3 * sizeof(float);
constexpr size_t kCropBytes = 5 * sizeof(uint32_t);
constexpr uint32_t kCropPadding = 16;

bool ValidPatch(const GroupImpostorPatch &patch, uint32_t side) {
  if (patch.Left >= patch.Right || patch.Top >= patch.Bottom || patch.Right > side ||
      patch.Bottom > side) {
    return false;
  }
  constexpr float rounding = 1e-6f;
  const float dx = patch.SlopeX * static_cast<float>(patch.Right - patch.Left);
  const float dy = patch.SlopeY * static_cast<float>(patch.Bottom - patch.Top);
  const std::array corners{patch.Depth, patch.Depth + dx, patch.Depth + dy, patch.Depth + dx + dy};
  return std::ranges::all_of(
      corners, [](float depth) { return depth >= -rounding && depth <= 1.0f + rounding; });
}

bool WritePatch(ByteWriter &out, const GroupImpostorPatch &p) {
  return out.Number(p.Left) && out.Number(p.Top) && out.Number(p.Right) && out.Number(p.Bottom) &&
         out.Number(p.Depth) && out.Number(p.SlopeX) && out.Number(p.SlopeY);
}

bool ReadPatch(ByteReader &in, GroupImpostorPatch &p) {
  return in.Number(p.Left) && in.Number(p.Top) && in.Number(p.Right) && in.Number(p.Bottom) &&
         in.Number(p.Depth) && in.Number(p.SlopeX) && in.Number(p.SlopeY);
}

bool WriteTexture(ByteWriter &writer, std::span<const GroupImpostorView::Texel> texture) {
  return writer.Put({reinterpret_cast<const uint8_t *>(texture.data()), texture.size_bytes()});
}

bool ReadTexture(ByteReader &reader,
                 size_t pixels,
                 std::vector<GroupImpostorView::Texel> &texture) {
  const auto bytes = reader.Take(pixels * sizeof(GroupImpostorView::Texel));
  if (!bytes) { return false; }
  texture.resize(pixels);
  std::memcpy(texture.data(), bytes->data(), bytes->size());
  return true;
}

bool CoversPatch(const GroupImpostorTextureRegion &region,
                 const GroupImpostorPatch &patch,
                 uint32_t side) {
  const auto padding = uint32_t{1} << (region.Levels - 1);
  const auto right = region.X + region.Width;
  const auto bottom = region.Y + region.Height;
  return patch.Left >= (region.X == 0 ? 0 : region.X + padding) &&
         patch.Top >= (region.Y == 0 ? 0 : region.Y + padding) &&
         patch.Right <= (right == side ? right : right - padding) &&
         patch.Bottom <= (bottom == side ? bottom : bottom - padding);
}

bool WriteRegion(ByteWriter &out, const GroupImpostorTextureRegion &region) {
  return out.Number(region.X) && out.Number(region.Y) && out.Number(region.Width) &&
         out.Number(region.Height) && out.Number(region.Levels);
}

bool ReadRegion(ByteReader &in, GroupImpostorTextureRegion &region) {
  return in.Number(region.X) && in.Number(region.Y) && in.Number(region.Width) &&
         in.Number(region.Height) && in.Number(region.Levels);
}

struct CropInterval {
  uint32_t Start, Length;
};

struct PatchInterval {
  uint32_t Low, High;
};

CropInterval PaddedInterval(PatchInterval bounds, uint32_t side) {
  const auto begin =
      bounds.Low > kCropPadding ? (bounds.Low - kCropPadding) / kCropPadding * kCropPadding : 0;
  const auto end = std::min(side, bounds.High + kCropPadding);
  const auto length = std::bit_ceil(std::max(kCropPadding, end - begin));
  return {.Start = std::min(begin, side - length), .Length = length};
}

void CropTexture(std::vector<GroupImpostorView::Texel> &texture,
                 uint32_t side,
                 const GroupImpostorTextureRegion &region) {
  std::vector<GroupImpostorView::Texel> retained;
  retained.reserve(region.Texels());
  size_t offset = 0;
  for (uint32_t level = 0; level < region.Levels; ++level) {
    const size_t stride = side >> level;
    const size_t width = region.Width >> level;
    const auto first = offset + (region.Y >> level) * stride + (region.X >> level);
    for (size_t row = 0; row < (region.Height >> level); ++row) {
      const auto texels = std::span(texture).subspan(first + row * stride, width);
      retained.insert(retained.end(), texels.begin(), texels.end());
    }
    offset += stride * stride;
  }
  texture = std::move(retained);
}

}

size_t GroupImpostorTextureRegion::Texels() const {
  size_t total = 0;
  for (uint32_t level = 0; level < Levels; ++level) {
    total += static_cast<size_t>(Width >> level) * (Height >> level);
  }
  return total;
}

bool GroupImpostorTextureRegion::Valid(uint32_t side) const {
  if (!GroupImpostorView::ValidSide(side) || !std::has_single_bit(Width) ||
      !std::has_single_bit(Height) || Width > side || Height > side || X > side - Width ||
      Y > side - Height || Levels == 0 ||
      std::cmp_greater(Levels, std::bit_width(std::min(Width, Height)))) {
    return false;
  }
  const auto alignment = uint32_t{1} << (Levels - 1);
  return X % alignment == 0 && Y % alignment == 0;
}

GroupImpostorTextureRegion GroupImpostorView::TextureRegion() const {
  return Crop.value_or(
      GroupImpostorTextureRegion{.X = 0,
                                 .Y = 0,
                                 .Width = Side,
                                 .Height = Side,
                                 .Levels = static_cast<uint32_t>(std::bit_width(Side))});
}

bool CropGroupImpostorView(GroupImpostorView &view) {
  if (view.Crop || view.Patches.empty() || !GroupImpostorView::ValidSide(view.Side) ||
      view.Side <= kCropPadding) {
    return false;
  }
  const size_t full = GroupImpostorView::MipTexels(view.Side);
  if (view.AlbedoCoverage.size() != full || view.NormalMaterial.size() != full) { return false; }
  uint32_t left = view.Side;
  uint32_t top = view.Side;
  uint32_t right = 0;
  uint32_t bottom = 0;
  for (const auto &patch : view.Patches) {
    if (!ValidPatch(patch, view.Side)) { return false; }
    left = std::min<uint32_t>(left, patch.Left);
    top = std::min<uint32_t>(top, patch.Top);
    right = std::max<uint32_t>(right, patch.Right);
    bottom = std::max<uint32_t>(bottom, patch.Bottom);
  }
  const auto x = PaddedInterval({.Low = left, .High = right}, view.Side);
  const auto y = PaddedInterval({.Low = top, .High = bottom}, view.Side);
  if (x.Length == view.Side && y.Length == view.Side) { return false; }
  const GroupImpostorTextureRegion region{.X = x.Start,
                                          .Y = y.Start,
                                          .Width = x.Length,
                                          .Height = y.Length,
                                          .Levels = std::bit_width(kCropPadding)};
  CropTexture(view.AlbedoCoverage, view.Side, region);
  CropTexture(view.NormalMaterial, view.Side, region);
  view.Crop = region;
  return true;
}

std::optional<std::vector<uint8_t>> EncodeGroupImpostorView(const GroupImpostorView &view) {
  const auto region = view.TextureRegion();
  if (!region.Valid(view.Side) || !ImpostorViewFrame::From(view.TowardEye)) { return std::nullopt; }
  const size_t pixels = region.Texels();
  if (view.AlbedoCoverage.size() != pixels || view.NormalMaterial.size() != pixels ||
      view.Patches.size() > static_cast<size_t>(view.Side) * view.Side) {
    return std::nullopt;
  }
  const size_t size = kHeaderBytes + (view.Crop ? kCropBytes : 0) +
                      kPatchBytes * view.Patches.size() +
                      2 * pixels * sizeof(GroupImpostorView::Texel);
  ByteWriter out(size);
  out.Reserve(size);
  const auto magic = view.Crop ? kCroppedMagic : kMagic;
  if (!out.Put({reinterpret_cast<const uint8_t *>(magic.data()), magic.size()}) ||
      !out.Number(view.Side) || !out.Number(static_cast<uint32_t>(view.Patches.size()))) {
    return std::nullopt;
  }
  for (const double axis : view.TowardEye) {
    if (!out.Number(axis)) { return std::nullopt; }
  }
  if (view.Crop && !WriteRegion(out, *view.Crop)) { return std::nullopt; }
  for (const auto &patch : view.Patches) {
    if (!ValidPatch(patch, view.Side) || !CoversPatch(region, patch, view.Side) ||
        !WritePatch(out, patch)) {
      return std::nullopt;
    }
  }
  if (!WriteTexture(out, view.AlbedoCoverage) || !WriteTexture(out, view.NormalMaterial)) {
    return std::nullopt;
  }
  return std::move(out).TakeBytes();
}

std::optional<GroupImpostorView> DecodeGroupImpostorView(std::span<const uint8_t> bytes) {
  ByteReader in(bytes);
  const auto magic = in.Take(kMagic.size());
  const bool cropped = magic && std::ranges::equal(*magic, kCroppedMagic);
  uint32_t patches = 0;
  GroupImpostorView result;
  if (!magic || (!cropped && !std::ranges::equal(*magic, kMagic)) || !in.Number(result.Side) ||
      !in.Number(patches) || !GroupImpostorView::ValidSide(result.Side) ||
      patches > result.Side * result.Side) {
    return std::nullopt;
  }
  for (double &axis : result.TowardEye) {
    if (!in.Number(axis)) { return std::nullopt; }
  }
  if (!ImpostorViewFrame::From(result.TowardEye)) { return std::nullopt; }
  if (cropped && !ReadRegion(in, result.Crop.emplace())) { return std::nullopt; }
  const auto region = result.TextureRegion();
  if (!region.Valid(result.Side)) { return std::nullopt; }
  const size_t pixels = region.Texels();
  if (bytes.size() != kHeaderBytes + (cropped ? kCropBytes : 0) + kPatchBytes * patches +
                          2 * pixels * sizeof(GroupImpostorView::Texel)) {
    return std::nullopt;
  }
  result.Patches.resize(patches);
  for (auto &patch : result.Patches) {
    if (!ReadPatch(in, patch) || !ValidPatch(patch, result.Side) ||
        !CoversPatch(region, patch, result.Side)) {
      return std::nullopt;
    }
  }
  if (!ReadTexture(in, pixels, result.AlbedoCoverage) ||
      !ReadTexture(in, pixels, result.NormalMaterial)) {
    return std::nullopt;
  }
  return result;
}

}
