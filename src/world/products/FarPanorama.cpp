#include "FarPanorama.h"
#include "BinaryValueArchive.h"
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
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine {
namespace {
using Json = nlohmann::json;
constexpr size_t kMetadataBytes = 65536;
constexpr size_t kHashLength = 64;
constexpr double kMaximumWorldRangeM = 240000;
constexpr uint32_t kPackedNearVersion = 2;
constexpr uint32_t kPackedVersion = 3;

bool ValidHeader(const FarPanorama &panorama) {
  return panorama.Side >= 2 && panorama.Side <= FarPanorama::MaximumSide &&
         std::abs(panorama.Origin.LongitudeDeg) <= kDegPerHalfTurn &&
         std::abs(panorama.Origin.LatitudeDeg) <= kDegPerHalfTurn / 2 &&
         std::ranges::all_of(
             panorama.AnchorM,
             [](double component) { return std::abs(component) <= Ground::kMercatorGirthM; }) &&
         panorama.MaximumRangeM > 0 && panorama.MaximumRangeM <= kMaximumWorldRangeM &&
         panorama.AxialNearM >= 0 && panorama.AxialNearM < panorama.MaximumRangeM &&
         (panorama.AxialOverlapEndM == 0 ||
          (panorama.AxialNearM > 0 && panorama.AxialOverlapEndM > panorama.AxialNearM &&
           panorama.AxialOverlapEndM <= panorama.MaximumRangeM)) &&
         panorama.MaximumTranslationM >= 0 &&
         panorama.MaximumTranslationM < panorama.MaximumRangeM && panorama.ReferenceExposure > 0 &&
         panorama.ReferenceExposure <= std::numeric_limits<float>::max() &&
         panorama.Revision.size() == kHashLength &&
         std::ranges::all_of(panorama.Revision,
                             [](unsigned char ch) { return std::isxdigit(ch) != 0; }) &&
         std::ranges::all_of(panorama.OpaqueTexels,
                             [&](size_t count) { return count <= panorama.Side * panorama.Side; });
}

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
  if (side < 2 || side > FarPanorama::MaximumSide) { return false; }
  panorama.Side = static_cast<size_t>(side);
  panorama.Origin = {.LongitudeDeg = origin[0].get<double>(),
                     .LatitudeDeg = origin[1].get<double>()};
  panorama.AnchorM = {{anchor[0].get<double>(), anchor[1].get<double>(), anchor[2].get<double>()}};
  if (const auto near = record.find("axial_near_m"); near != record.end()) {
    if (!near->is_number()) { return false; }
    panorama.AxialNearM = near->get<double>();
  }
  if (const auto overlap = record.find("axial_overlap_end_m"); overlap != record.end()) {
    if (!overlap->is_number()) { return false; }
    panorama.AxialOverlapEndM = overlap->get<double>();
  }
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
  return ValidHeader(panorama);
}

std::expected<std::string, std::string> ReadAttachment(const std::filesystem::path &path,
                                                       size_t bytes) {
  auto content = ReadTextFile(path.string(), bytes);
  if (!content) { return std::unexpected(content.error()); }
  if (content->size() != bytes) { return std::unexpected("far panorama attachment is truncated"); }
  return content;
}

bool ReadPackedField(BinaryValueReader &in, size_t count, std::string &field) {
  uint32_t length = 0;
  if (!in(length) || length != count) { return false; }
  const auto bytes = in.In.Take(count);
  if (!bytes) { return false; }
  field.assign(reinterpret_cast<const char *>(bytes->data()), count);
  return true;
}
}

bool PanoramaDomain::Covers(const Vec3 &eye) const {
  return Length(eye - AnchorM) <= RadiusM + RoundoffM;
}

std::array<double, 2> PanoramaDomain::Along(const PanoramaRay &ray) const {
  const auto offset = AnchorM - ray.OriginM;
  const double centre = Dot(offset, ray.Direction);
  const auto across = offset - ray.Direction * centre;
  const double radius = RadiusM + RoundoffM;
  const double half = std::sqrt(std::max(0.0, radius * radius - Dot(across, across)));
  return {centre - half, centre + half};
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

std::expected<std::vector<uint8_t>, std::string> EncodeFarPanorama(const FarPanorama &panorama) {
  if (!ValidHeader(panorama)) { return std::unexpected("invalid far panorama header"); }
  const size_t texels = FarPanorama::FaceCount * panorama.Side * panorama.Side;
  if (panorama.Colour.size() != texels * FarPanorama::ColourBytesPerTexel ||
      panorama.Depth.size() != texels * FarPanorama::DepthBytesPerTexel) {
    return std::unexpected("invalid far panorama attachment sizes");
  }
  std::array<uint32_t, FarPanorama::FaceCount> opaque{};
  for (size_t face = 0; face < opaque.size(); ++face) {
    opaque[face] = static_cast<uint32_t>(panorama.OpaqueTexels[face]);
  }
  BinaryValueWriter out(FarPanorama::MaximumPackedBytes);
  out.Out.Reserve(panorama.Colour.size() + panorama.Depth.size() + FarPanorama::PackedHeaderBytes);
  if (!out(kPackedVersion,
           static_cast<uint32_t>(panorama.Side),
           panorama.Origin.LongitudeDeg,
           panorama.Origin.LatitudeDeg,
           panorama.AnchorM.Axis,
           panorama.MaximumRangeM,
           panorama.MaximumTranslationM,
           panorama.ReferenceExposure,
           opaque,
           panorama.AxialNearM,
           panorama.AxialOverlapEndM) ||
      !out.Text(panorama.Revision) || !out.Text(panorama.Colour) || !out.Text(panorama.Depth)) {
    return std::unexpected("far panorama could not be packed");
  }
  return std::move(out.Out).TakeBytes();
}

std::expected<FarPanorama, std::string> DecodeFarPanorama(std::span<const uint8_t> bytes) {
  if (bytes.size() > FarPanorama::MaximumPackedBytes) {
    return std::unexpected("packed far panorama exceeds extent limit");
  }
  BinaryValueReader in(bytes);
  uint32_t version = 0;
  uint32_t side = 0;
  std::array<uint32_t, FarPanorama::FaceCount> opaque{};
  FarPanorama panorama;
  if (!in(version, side) || version < 1 || version > kPackedVersion ||
      !in(panorama.Origin.LongitudeDeg,
          panorama.Origin.LatitudeDeg,
          panorama.AnchorM.Axis,
          panorama.MaximumRangeM,
          panorama.MaximumTranslationM,
          panorama.ReferenceExposure,
          opaque) ||
      (version >= kPackedNearVersion && !in(panorama.AxialNearM)) ||
      (version >= kPackedVersion && !in(panorama.AxialOverlapEndM)) ||
      !ReadPackedField(in, kHashLength, panorama.Revision)) {
    return std::unexpected("invalid packed far panorama header");
  }
  panorama.Side = side;
  std::ranges::copy(opaque, panorama.OpaqueTexels.begin());
  if (!ValidHeader(panorama)) { return std::unexpected("invalid packed far panorama domain"); }
  const size_t texels = FarPanorama::FaceCount * panorama.Side * panorama.Side;
  if (!ReadPackedField(in, texels * FarPanorama::ColourBytesPerTexel, panorama.Colour) ||
      !ReadPackedField(in, texels * FarPanorama::DepthBytesPerTexel, panorama.Depth) ||
      in.In.Remaining() != 0) {
    return std::unexpected("invalid packed far panorama attachment sizes");
  }
  return panorama;
}
}
