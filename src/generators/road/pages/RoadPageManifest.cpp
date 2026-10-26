#include "RoadPageManifest.h"
#include "BinaryValueArchive.h"
#include "Sha256.h"
#include "TangentFrame.h"
#include "Units.h"
#include <GeometryProductCodec.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>
#include <variant>

namespace outshine::Generators {
namespace {
constexpr uint64_t kLegacyFormat = 0x0001314d4452ULL;
constexpr uint64_t kAppearanceFormat = 0x0002314d4452ULL;
constexpr size_t kAppearanceBytes = sizeof(uint32_t) + 64;
constexpr size_t kReferenceBytes =
    sizeof(uint32_t) + 64 + 6 * sizeof(double) + sizeof(uint64_t) + 2 * sizeof(uint32_t);

bool Key(const std::string &key) {
  return key.size() == 64 && std::ranges::all_of(key, [](char value) {
           return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f');
         });
}

bool Valid(const RoadPageManifest &manifest) {
  if (!Key(manifest.PlanKey) || !Key(manifest.MaterialKey) ||
      !ValidRoadPageAppearance(manifest.Appearance) ||
      !std::isfinite(manifest.Anchor.LatitudeDeg) || !std::isfinite(manifest.Anchor.LongitudeDeg) ||
      std::abs(manifest.Anchor.LatitudeDeg) > kDegPerHalfTurn * 0.5 ||
      std::abs(manifest.Anchor.LongitudeDeg) > kDegPerHalfTurn || manifest.Materials.parts() != 0 ||
      manifest.Materials.surfaces() != 1 || manifest.Materials.images() != 0 ||
      manifest.Pages.size() > std::numeric_limits<uint32_t>::max()) {
    return false;
  }
  if (const auto *appearance = std::get_if<RoadPageAppearanceV2>(&manifest.Appearance);
      appearance != nullptr && appearance->Key != RoadPageManifestAppearanceKey(manifest.Pages)) {
    return false;
  }
  std::unordered_set<std::string> keys;
  for (const auto &page : manifest.Pages) {
    if (!Key(page.Key) || !keys.insert(page.Key).second || page.ByteCount == 0 ||
        page.Appearance.index() != manifest.Appearance.index() ||
        RoadPageKey({.PlanKey = manifest.PlanKey,
                     .MaterialKey = manifest.MaterialKey,
                     .MaterialIndex = 0,
                     .Anchor = manifest.Anchor,
                     .Level = page.Level,
                     .Parts = page.Parts,
                     .Appearance = page.Appearance}) != page.Key) {
      return false;
    }
    for (size_t axis = 0; axis < 3; ++axis) {
      if (!std::isfinite(page.Bounds.Min[axis]) || !std::isfinite(page.Bounds.Max[axis]) ||
          page.Bounds.Min[axis] > page.Bounds.Max[axis] ||
          std::abs(page.Bounds.Min[axis]) > std::numeric_limits<float>::max() ||
          std::abs(page.Bounds.Max[axis]) > std::numeric_limits<float>::max()) {
        return false;
      }
    }
  }
  return true;
}

bool ReferenceFields(BinaryValueWriter &archive, const RoadPageReference &page) {
  if (page.Parts.size() > std::numeric_limits<uint32_t>::max() || !archive.Text(page.Key) ||
      !WriteRoadPageAppearance(archive, page.Appearance) ||
      !archive(page.Bounds.Min.Axis,
               page.Bounds.Max.Axis,
               page.ByteCount,
               page.Level,
               static_cast<uint32_t>(page.Parts.size()))) {
    return false;
  }
  return std::ranges::all_of(page.Parts,
                             [&](const auto &part) { return archive(part.Kind, part.Index); });
}

bool ReferenceFields(BinaryValueReader &archive, RoadPageReference &page, bool version2) {
  uint32_t count = 0;
  if (!archive.Text(page.Key) || !ReadRoadPageAppearance(archive, version2, page.Appearance) ||
      !archive(page.Bounds.Min.Axis, page.Bounds.Max.Axis, page.ByteCount, page.Level, count) ||
      count > archive.In.Remaining() / (2 * sizeof(uint32_t))) {
    return false;
  }
  page.Parts.resize(count);
  return std::ranges::all_of(page.Parts,
                             [&](auto &part) { return archive(part.Kind, part.Index); });
}
}

std::string RoadPageManifestAppearanceKey(std::span<const RoadPageReference> pages) {
  constexpr size_t prefixBytes = 64;
  constexpr size_t keyBytes = sizeof(uint32_t) + 64;
  if (pages.size() > std::numeric_limits<uint32_t>::max() ||
      pages.size() > (std::numeric_limits<size_t>::max() - prefixBytes) / keyBytes) {
    return {};
  }
  BinaryValueWriter out(prefixBytes + pages.size() * keyBytes);
  if (!out.Text("outshine-road-manifest-appearance") ||
      !out(uint32_t{2}, static_cast<uint32_t>(pages.size()))) {
    return {};
  }
  for (const auto &page : pages) {
    if (!std::holds_alternative<RoadPageAppearanceV2>(page.Appearance) ||
        !WriteRoadPageAppearance(out, page.Appearance)) {
      return {};
    }
  }
  return Sha256Hex(out.Out.Bytes().data(), out.Out.Bytes().size());
}

std::optional<std::vector<uint8_t>> EncodeRoadPageManifest(const RoadPageManifest &manifest,
                                                           size_t bytesMost) {
  if (!Valid(manifest)) { return std::nullopt; }
  const auto materials = EncodeGeometryProduct(manifest.Materials, bytesMost);
  if (!materials || Sha256Hex(materials->data(), materials->size()) != manifest.MaterialKey) {
    return std::nullopt;
  }
  BinaryValueWriter out(bytesMost);
  const auto format = std::holds_alternative<LegacyRoadPageV1>(manifest.Appearance)
                          ? kLegacyFormat
                          : kAppearanceFormat;
  if (!out(format) || !out.Text(manifest.PlanKey) || !out.Text(manifest.MaterialKey) ||
      !WriteRoadPageAppearance(out, manifest.Appearance) ||
      !out(manifest.Anchor.LongitudeDeg,
           manifest.Anchor.LatitudeDeg,
           static_cast<uint32_t>(manifest.Pages.size())) ||
      !std::ranges::all_of(manifest.Pages,
                           [&](const auto &page) { return ReferenceFields(out, page); }) ||
      !out.Array<uint8_t>(*materials)) {
    return std::nullopt;
  }
  return std::move(out.Out).TakeBytes();
}

std::optional<RoadPageManifest> DecodeRoadPageManifest(std::span<const uint8_t> bytes,
                                                       size_t bytesMost) {
  if (bytes.size() > bytesMost) { return std::nullopt; }
  BinaryValueReader in(bytes);
  RoadPageManifest manifest;
  uint64_t format = 0;
  uint32_t count = 0;
  if (!in(format) || (format != kLegacyFormat && format != kAppearanceFormat) ||
      !in.Text(manifest.PlanKey) || !in.Text(manifest.MaterialKey) ||
      !ReadRoadPageAppearance(in, format == kAppearanceFormat, manifest.Appearance) ||
      !in(manifest.Anchor.LongitudeDeg, manifest.Anchor.LatitudeDeg, count) ||
      count > in.In.Remaining() /
                  (kReferenceBytes + (format == kAppearanceFormat ? kAppearanceBytes : 0))) {
    return std::nullopt;
  }
  manifest.Pages.resize(count);
  if (!std::ranges::all_of(manifest.Pages, [&](auto &page) {
        return ReferenceFields(in, page, format == kAppearanceFormat);
      })) {
    return std::nullopt;
  }
  std::vector<uint8_t> materials;
  if (!in.Array(materials) || in.In.Remaining() != 0 ||
      Sha256Hex(materials.data(), materials.size()) != manifest.MaterialKey) {
    return std::nullopt;
  }
  auto decoded = DecodeGeometryProduct(materials, bytesMost);
  if (!decoded) { return std::nullopt; }
  manifest.Materials = std::move(*decoded);
  if (!Valid(manifest)) { return std::nullopt; }
  return manifest;
}

std::string RoadPageManifestKey(const RoadPageManifest &manifest, size_t bytesMost) {
  const auto bytes = EncodeRoadPageManifest(manifest, bytesMost);
  return bytes ? Sha256Hex(bytes->data(), bytes->size()) : std::string{};
}

Box RoadPageManifestBounds(const RoadPageManifest &manifest) {
  Box bounds;
  for (const auto &page : manifest.Pages) { bounds.Cover(page.Bounds); }
  if (manifest.Pages.empty()) { bounds.Cover(TangentFrame::At(manifest.Anchor).OriginEcef()); }
  return bounds;
}
}
