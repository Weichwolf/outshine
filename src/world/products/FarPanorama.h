#ifndef OUTSHINE_WORLD_PRODUCTS_FARPANORAMA_H
#define OUTSHINE_WORLD_PRODUCTS_FARPANORAMA_H

#include "Earth.h"
#include "Vec3.h"
#include <array>
#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

namespace outshine {

struct FarPanorama {
  static constexpr std::string_view MetadataFile = "panorama.json";
  static constexpr std::string_view ColourFile = "colour.rgba16f";
  static constexpr std::string_view DepthFile = "depth.r32f";
  static constexpr size_t FaceCount = 6;
  static constexpr size_t ColourBytesPerTexel = 8;
  static constexpr size_t DepthBytesPerTexel = 4;

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
};

[[nodiscard]] std::expected<FarPanorama, std::string> ReadFarPanorama(std::string_view directory);

}
#endif
