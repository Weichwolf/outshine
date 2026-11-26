#include "FarPanorama.h"
#include "ReadTextFile.h"
#include "Units.h"
#include "TileGeodesy.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <limits>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <utility>

namespace outshine {
namespace {
using Json = nlohmann::json;
constexpr size_t kMetadataBytes = 65536;
constexpr size_t kMaximumSide = 2048;
constexpr size_t kHashLength = 64;
constexpr double kMaximumWorldRangeM = 240000;

bool BoundedVector(const Json &vector, size_t size, double limit) {
  return vector.is_array() && vector.size() == size &&
         std::ranges::all_of(vector, [limit](const Json &item) {
           return item.is_number() && std::abs(item.get<double>()) <= limit;
         });
}

bool HasEncodings(const Json &record) {
  constexpr std::array<std::string_view, FarPanorama::FaceCount> kOrder{
      "px", "nx", "py", "ny", "pz", "nz"};
  return record.contains("version") && record["version"] == 1 &&
         record.contains("representation") &&
         record["representation"] == "colour_radial_depth_cubemap" &&
         record.contains("face_order") && record["face_order"] == kOrder &&
         record.contains("colour_encoding") &&
         record["colour_encoding"] == "rgba16f_linear_le_a_opaque_coverage" &&
         record.contains("depth_encoding") &&
         record["depth_encoding"] == "r32f_radial_metres_le_zero_without_geometry";
}

bool SetHeader(const Json &record, FarPanorama &panorama) {
  constexpr std::array names{"face_px",
                             "origin_lon_lat",
                             "anchor_eus_m",
                             "maximum_range_m",
                             "maximum_eye_translation_m",
                             "reference_exposure",
                             "opaque_texels",
                             "revision"};
  if (!HasEncodings(record) ||
      !std::ranges::all_of(names, [&](const char *key) { return record.contains(key); })) {
    return false;
  }
  const auto &origin = record["origin_lon_lat"];
  const auto &anchor = record["anchor_eus_m"];
  const auto &counts = record["opaque_texels"];
  if (!record["face_px"].is_number_unsigned() || !BoundedVector(origin, 2, kDegPerHalfTurn) ||
      !BoundedVector(anchor, 3, Ground::kMercatorGirthM) ||
      !record["maximum_range_m"].is_number() || !record["maximum_eye_translation_m"].is_number() ||
      !record["reference_exposure"].is_number() || !record["revision"].is_string() ||
      !counts.is_array() || counts.size() != FarPanorama::FaceCount) {
    return false;
  }
  const auto side = record["face_px"].get<uint64_t>();
  if (side < 2 || side > kMaximumSide) { return false; }
  panorama.Side = static_cast<size_t>(side);
  panorama.Origin = {.LongitudeDeg = origin[0].get<double>(),
                     .LatitudeDeg = origin[1].get<double>()};
  panorama.AnchorM = {{anchor[0].get<double>(), anchor[1].get<double>(), anchor[2].get<double>()}};
  panorama.MaximumRangeM = record["maximum_range_m"].get<double>();
  panorama.MaximumTranslationM = record["maximum_eye_translation_m"].get<double>();
  panorama.ReferenceExposure = record["reference_exposure"].get<double>();
  panorama.Revision = record["revision"].get<std::string>();
  for (size_t face = 0; face < FarPanorama::FaceCount; ++face) {
    if (!counts[face].is_number_unsigned() || counts[face].get<uint64_t>() > side * side) {
      return false;
    }
    panorama.OpaqueTexels[face] = counts[face].get<size_t>();
  }
  return std::abs(panorama.Origin.LatitudeDeg) <= kDegPerHalfTurn / 2 &&
         panorama.MaximumRangeM > 0 && panorama.MaximumRangeM <= kMaximumWorldRangeM &&
         panorama.MaximumTranslationM >= 0 &&
         panorama.MaximumTranslationM < panorama.MaximumRangeM && panorama.ReferenceExposure > 0 &&
         panorama.ReferenceExposure <= std::numeric_limits<float>::max() &&
         panorama.Revision.size() == kHashLength &&
         std::ranges::all_of(panorama.Revision,
                             [](unsigned char ch) { return std::isxdigit(ch) != 0; });
}

std::expected<std::string, std::string> ReadAttachment(const std::filesystem::path &path,
                                                       size_t bytes) {
  auto content = ReadTextFile(path.string(), bytes);
  if (!content) { return std::unexpected(content.error()); }
  if (content->size() != bytes) { return std::unexpected("far panorama attachment is truncated"); }
  return content;
}
}

std::expected<FarPanorama, std::string> ReadFarPanorama(std::string_view directory) {
  const std::filesystem::path root(directory);
  const auto meta = ReadTextFile((root / FarPanorama::MetadataFile).string(), kMetadataBytes);
  if (!meta) { return std::unexpected(meta.error()); }
  const auto record = Json::parse(*meta, nullptr, false);
  FarPanorama panorama;
  if (!record.is_object() || !SetHeader(record, panorama)) {
    return std::unexpected("invalid far panorama header");
  }
  const size_t texels = FarPanorama::FaceCount * panorama.Side * panorama.Side;
  auto colour =
      ReadAttachment(root / FarPanorama::ColourFile, texels * FarPanorama::ColourBytesPerTexel);
  if (!colour) { return std::unexpected(colour.error()); }
  auto depth =
      ReadAttachment(root / FarPanorama::DepthFile, texels * FarPanorama::DepthBytesPerTexel);
  if (!depth) { return std::unexpected(depth.error()); }
  panorama.Colour = std::move(*colour);
  panorama.Depth = std::move(*depth);
  return panorama;
}
}
