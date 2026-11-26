#ifndef OUTSHINE_WORLD_PRODUCTS_IMPOSTOR_GROUPIMPOSTORVIEW_H
#define OUTSHINE_WORLD_PRODUCTS_IMPOSTOR_GROUPIMPOSTORVIEW_H

#include "Vec3.h"
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace outshine {

struct GroupImpostorPatch {
  uint16_t Left, Top, Right, Bottom;
  float Depth, SlopeX, SlopeY;
};

struct GroupImpostorTextureRegion {
  uint32_t X, Y, Width, Height, Levels;

  [[nodiscard]] size_t Texels() const;
  [[nodiscard]] bool Valid(uint32_t side) const;
};

struct GroupImpostorView {
  using Texel = std::array<uint8_t, 4>;
  static constexpr uint32_t MaximumSide = 256;
  static constexpr size_t MaximumViews = 64;

  [[nodiscard]] static constexpr bool ValidSide(uint32_t side) {
    return side <= MaximumSide && std::has_single_bit(side);
  }

  [[nodiscard]] static constexpr size_t MipTexels(uint32_t side) {
    return (4 * static_cast<size_t>(side) * side - 1) / 3;
  }

  Vec3 TowardEye;
  uint32_t Side = 0;
  std::vector<GroupImpostorPatch> Patches;
  std::vector<Texel> AlbedoCoverage;
  std::vector<Texel> NormalMaterial;
  std::optional<GroupImpostorTextureRegion> Crop = std::nullopt;

  [[nodiscard]] GroupImpostorTextureRegion TextureRegion() const;
};

[[nodiscard]] bool CropGroupImpostorView(GroupImpostorView &view);

[[nodiscard]] std::optional<std::vector<uint8_t>>
EncodeGroupImpostorView(const GroupImpostorView &view);
[[nodiscard]] std::optional<GroupImpostorView>
DecodeGroupImpostorView(std::span<const uint8_t> bytes);

}
#endif
