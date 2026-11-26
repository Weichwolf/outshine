#include "GroupImpostorView.h"
#include "ByteArchive.h"
#include "ImpostorViewFrame.h"
#include <algorithm>
#include <array>
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
constexpr size_t kHeaderBytes = kMagic.size() + 2 * sizeof(uint32_t) + 3 * sizeof(double);
constexpr size_t kPatchBytes = 4 * sizeof(uint16_t) + 3 * sizeof(float);

size_t TextureBytes(uint32_t side) {
  return GroupImpostorView::MipTexels(side) * sizeof(GroupImpostorView::Texel);
}

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
                 uint32_t side,
                 std::vector<GroupImpostorView::Texel> &texture) {
  const auto bytes = reader.Take(TextureBytes(side));
  if (!bytes) { return false; }
  texture.resize(GroupImpostorView::MipTexels(side));
  std::memcpy(texture.data(), bytes->data(), bytes->size());
  return true;
}

}

std::optional<std::vector<uint8_t>> EncodeGroupImpostorView(const GroupImpostorView &view) {
  if (!GroupImpostorView::ValidSide(view.Side) || !ImpostorViewFrame::From(view.TowardEye)) {
    return std::nullopt;
  }
  const size_t pixels = GroupImpostorView::MipTexels(view.Side);
  if (view.AlbedoCoverage.size() != pixels || view.NormalMaterial.size() != pixels ||
      view.Patches.size() > static_cast<size_t>(view.Side) * view.Side) {
    return std::nullopt;
  }
  const size_t size =
      kHeaderBytes + kPatchBytes * view.Patches.size() + 2 * TextureBytes(view.Side);
  ByteWriter out(size);
  out.Reserve(size);
  if (!out.Put({reinterpret_cast<const uint8_t *>(kMagic.data()), kMagic.size()}) ||
      !out.Number(view.Side) || !out.Number(static_cast<uint32_t>(view.Patches.size()))) {
    return std::nullopt;
  }
  for (const double axis : view.TowardEye) {
    if (!out.Number(axis)) { return std::nullopt; }
  }
  for (const auto &patch : view.Patches) {
    if (!ValidPatch(patch, view.Side) || !WritePatch(out, patch)) { return std::nullopt; }
  }
  if (!WriteTexture(out, view.AlbedoCoverage) || !WriteTexture(out, view.NormalMaterial)) {
    return std::nullopt;
  }
  return std::move(out).TakeBytes();
}

std::optional<GroupImpostorView> DecodeGroupImpostorView(std::span<const uint8_t> bytes) {
  ByteReader in(bytes);
  const auto magic = in.Take(kMagic.size());
  uint32_t patches = 0;
  GroupImpostorView result;
  if (!magic || !std::ranges::equal(*magic, kMagic) || !in.Number(result.Side) ||
      !in.Number(patches) || !GroupImpostorView::ValidSide(result.Side) ||
      patches > result.Side * result.Side ||
      bytes.size() != kHeaderBytes + kPatchBytes * patches + 2 * TextureBytes(result.Side)) {
    return std::nullopt;
  }
  for (double &axis : result.TowardEye) {
    if (!in.Number(axis)) { return std::nullopt; }
  }
  if (!ImpostorViewFrame::From(result.TowardEye)) { return std::nullopt; }
  result.Patches.resize(patches);
  for (auto &patch : result.Patches) {
    if (!ReadPatch(in, patch) || !ValidPatch(patch, result.Side)) { return std::nullopt; }
  }
  if (!ReadTexture(in, result.Side, result.AlbedoCoverage) ||
      !ReadTexture(in, result.Side, result.NormalMaterial)) {
    return std::nullopt;
  }
  return result;
}

}
