#ifndef OUTSHINE_WORLD_PRODUCTS_FARPANORAMA_H
#define OUTSHINE_WORLD_PRODUCTS_FARPANORAMA_H

#include "Earth.h"
#include "Vec3.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace outshine {

struct PanoramaRay {
  Vec3 OriginM;
  Vec3 Direction;
};

struct PanoramaDomain {
  static constexpr double RoundoffM = 1e-6;
  Vec3 AnchorM;
  double RadiusM = 0;

  [[nodiscard]] bool Covers(const Vec3 &eye) const;
  [[nodiscard]] std::array<double, 2> Along(const PanoramaRay &ray) const;
};

struct FarPanorama {
  static constexpr std::string_view MetadataFile = "panorama.json";
  static constexpr std::string_view ColourFile = "colour.rgba16f";
  static constexpr std::string_view DepthFile = "depth.r32f";
  static constexpr size_t FaceCount = 6;
  static constexpr size_t ColourBytesPerTexel = 8;
  static constexpr size_t DepthBytesPerTexel = 4;
  static constexpr size_t MaximumSide = 2048;
  static constexpr size_t MaximumPayloadBytes =
      MaximumSide * MaximumSide * FaceCount * (ColourBytesPerTexel + DepthBytesPerTexel);
  static constexpr size_t PackedHeaderBytes =
      (2 + FaceCount + 3) * sizeof(uint32_t) + 8 * sizeof(double) + 64;
  static constexpr size_t MaximumPackedBytes = MaximumPayloadBytes + PackedHeaderBytes;

  size_t Side = 0;
  LongitudeLatitude Origin;
  Vec3 AnchorM;
  double MaximumRangeM = 0;
  double MaximumTranslationM = 0;
  double ReferenceExposure = 0;
  std::string Revision;
  std::array<size_t, FaceCount> OpaqueTexels{};
  std::string Colour;
  std::string Depth;

  [[nodiscard]] PanoramaDomain Domain() const {
    return {.AnchorM = AnchorM, .RadiusM = MaximumTranslationM};
  }
};

[[nodiscard]] std::expected<FarPanorama, std::string> ReadFarPanorama(std::string_view directory);
[[nodiscard]] std::expected<std::vector<uint8_t>, std::string>
EncodeFarPanorama(const FarPanorama &panorama);
[[nodiscard]] std::expected<FarPanorama, std::string>
DecodeFarPanorama(std::span<const uint8_t> bytes);

}
#endif
