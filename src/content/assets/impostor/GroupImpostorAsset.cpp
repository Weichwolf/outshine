#include "GroupImpostorAsset.h"
#include "AssetCache.h"
#include "AssetPublication.h"
#include "GroupImpostorManifest.h"
#include "GroupImpostorView.h"
#include "ImpostorViewDemand.h"
#include "ReadTextFile.h"
#include "Sha256.h"
#include "SurfaceTile.h"
#include "TangentFrame.h"
#include "Vec3.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <nlohmann/json.hpp>

namespace outshine {
namespace {
using Json = nlohmann::json;

std::span<const uint8_t> Bytes(std::string_view s) {
  return {reinterpret_cast<const uint8_t *>(s.data()), s.size()};
}

std::expected<PreparedAssetWrite, std::string>
PreparePackage(AssetCache &cache, const AssetRecord &record, std::span<const uint8_t> bytes) {
  auto prepared = cache.PreparePublication({record}, {bytes.begin(), bytes.end()});
  if (!prepared) { return std::unexpected("could not prepare group impostor package"); }
  return std::move(*prepared);
}

bool MatchesView(const GroupImpostorView &view, const GroupImpostorManifest &manifest, size_t at) {
  return view.Side == manifest.Side && view.TowardEye == manifest.Directions[at];
}

std::string PackageKey(std::span<const uint8_t> bytes) {
  return Sha256Hex(bytes.data(), bytes.size());
}

std::expected<AssetRecord, std::string> StageView(AssetCache &cache,
                                                  const std::filesystem::path &path,
                                                  const GroupImpostorManifest &manifest,
                                                  size_t at,
                                                  std::vector<PreparedAssetWrite> &writes) {
  const auto file = ReadTextFile(path.string(), GroupImpostorViewBytesMost);
  if (!file) { return std::unexpected(file.error()); }
  const auto view = DecodeGroupImpostorView(Bytes(*file));
  if (!view || !MatchesView(*view, manifest, at)) {
    return std::unexpected("group view differs from its capture manifest");
  }
  const auto payload = PackageKey(Bytes(*file));
  const auto owner = GroupImpostorRequestKey(manifest.SurfaceRevision, manifest.Coverage);
  AssetRecord record{.Key =
                         Sha256Hex(std::format("{}/{}/{}", GroupImpostorViewKind, owner, payload)),
                     .Kind = std::string(GroupImpostorViewKind),
                     .Bounds = GroupImpostorEarthBounds(manifest),
                     .Package = payload,
                     .ByteCount = file->size(),
                     .Level = static_cast<uint32_t>(manifest.Coverage.Zoom),
                     .Parent = {}};
  auto write = PreparePackage(cache, record, Bytes(*file));
  if (!write) { return std::unexpected(write.error()); }
  writes.push_back(std::move(*write));
  return record;
}

std::expected<std::optional<CachedAsset>, std::string>
ReadManifest(const AssetCache &cache, std::string_view revision, Data::TileId tile) {
  auto manifest =
      cache.LoadRequest(GroupImpostorRequestKey(revision, tile), GroupImpostorManifestBytesMost);
  if (!manifest) {
    if (manifest.error() == AssetCacheError::CapacityExceeded) { return std::nullopt; }
    return std::unexpected("group impostor manifest lookup failed");
  }
  if (*manifest && (**manifest).Record().Kind != GroupImpostorManifestKind) { return std::nullopt; }
  return std::move(*manifest);
}

ImpostorEyeSweep LocalEye(const GroupImpostorManifest &manifest, const ImpostorEyeSweep &sweep) {
  const auto frame = TangentFrame::At(manifest.Origin);
  const auto local = [&](const Vec3 &ecef) {
    const auto p = frame.ToLocalPosition(ecef);
    return Vec3{{p.EastM, p.UpM, -p.NorthM}} - manifest.CentreM;
  };
  return {.Begin = local(sweep.Begin), .End = local(sweep.End)};
}

double DistanceToSweep(const ImpostorEyeSweep &eye) {
  const auto segment = eye.End - eye.Begin;
  const double squared = Dot(segment, segment);
  const double t = squared > 0 ? std::clamp(-Dot(eye.Begin, segment) / squared, 0.0, 1.0) : 0;
  return Length(eye.Begin + segment * t);
}

bool FitsProjection(const GroupImpostorManifest &group, const ImpostorEyeSweep &eye, double focal) {
  constexpr double kMaxProjectedTexelPx = 1.0;
  const double clear = DistanceToSweep(eye) - std::numbers::sqrt3 * group.HalfExtentM;
  const double texel = 2.0 * group.HalfExtentM / group.Side;
  return clear > 0 && focal > 0 && texel * focal <= clear * kMaxProjectedTexelPx;
}

size_t OverlayBytes(const SurfaceTile &surface) {
  const size_t ground = static_cast<size_t>(surface.GridSide) * surface.GridSide;
  if (ground == 0 || ground >= surface.Vertices.size() ||
      surface.GroundIndices >= surface.Indices.size()) {
    return 0;
  }
  const size_t vertices = surface.Vertices.size() - ground;
  return vertices * sizeof(SurfaceVertex) +
         std::min(vertices, surface.Colours.size()) * sizeof(SurfaceColour) +
         (surface.Indices.size() - surface.GroundIndices) * sizeof(uint32_t);
}

std::expected<std::vector<AssetRecord>, std::string> ReadViewRecords(const AssetCache &cache,
                                                                     const Json &keys,
                                                                     std::span<const size_t> needed,
                                                                     size_t limit) {
  std::vector<AssetRecord> result;
  result.reserve(needed.size());
  for (const auto index : needed) {
    if (!keys[index].is_string()) { return std::vector<AssetRecord>{}; }
    const auto &key = keys[index].get_ref<const std::string &>();
    if (!AssetProductKey::Parse(key)) { return std::vector<AssetRecord>{}; }
    auto record = cache.Find(key);
    if (!record) { return std::unexpected("group impostor view index lookup failed"); }
    if (!*record || (**record).Kind != GroupImpostorViewKind ||
        (**record).ByteCount > GroupImpostorViewBytesMost || (**record).ByteCount >= limit) {
      return std::vector<AssetRecord>{};
    }
    limit -= static_cast<size_t>((**record).ByteCount);
    result.push_back(std::move(**record));
  }
  return result;
}

}

std::expected<void, std::string> ImportGroupImpostor(AssetCache &cache,
                                                     const GroupImpostorImport &source) {
  if (!AssetProductKey::Parse(source.Revision)) {
    return std::unexpected("invalid render set revision");
  }
  const std::filesystem::path folder(source.Directory);
  const auto input = ReadTextFile((folder / "group.json").string(), GroupImpostorManifestBytesMost);
  if (!input) { return std::unexpected(input.error()); }
  auto manifestJson = Json::parse(*input, nullptr, false);
  const auto manifest = ParseGroupImpostorManifest(manifestJson);
  if (!manifest) { return std::unexpected("invalid group impostor capture manifest"); }
  std::vector<PreparedAssetWrite> writes;
  writes.reserve(manifest->Directions.size() + 1);
  auto cards = Json::array();
  for (size_t at = 0; at < manifest->Directions.size(); ++at) {
    const auto card = StageView(cache, folder / std::format("{}.card", at), *manifest, at, writes);
    if (!card) { return std::unexpected(card.error()); }
    cards.push_back(card->Key);
  }
  manifestJson["card_assets"] = std::move(cards);
  const auto body = manifestJson.dump();
  if (body.size() > GroupImpostorManifestBytesMost) {
    return std::unexpected("oversized group impostor manifest");
  }
  const auto request = GroupImpostorRequestKey(source.Revision, manifest->Coverage);
  const auto package = PackageKey(Bytes(body));
  const AssetRecord index{
      .Key = Sha256Hex(request + package),
      .Kind = std::string(GroupImpostorManifestKind),
      .Bounds = GroupImpostorEarthBounds(*manifest),
      .Package = package,
      .ByteCount = body.size(),
      .Level = static_cast<uint32_t>(manifest->Coverage.Zoom),
      .Parent = {},
      .RequestKey = request,
      .Tile =
          AssetTile{.Revision = std::string(source.Revision), .Coverage = request, .Coarser = {}}};
  auto write = PreparePackage(cache, index, Bytes(body));
  if (!write) { return std::unexpected(write.error()); }
  writes.push_back(std::move(*write));
  if (!cache.PublishBatch(writes)) { return std::unexpected("could not publish group impostor"); }
  return {};
}

std::expected<GroupImpostorLookup, std::string>
LoadGroupImpostor(const AssetCache &cache,
                  std::string_view revision,
                  const SurfaceTile &surface,
                  const GroupImpostorDemand &demand) {
  GroupImpostorLookup result;
  const size_t meshBytes = OverlayBytes(surface);
  if (meshBytes == 0) { return result; }
  const auto loaded = ReadManifest(cache, revision, surface.Coverage);
  if (!loaded) { return std::unexpected(loaded.error()); }
  if (!*loaded) { return result; }
  const auto content = (**loaded).Bytes();
  result.RequestedBytes = content.size();
  const auto json = Json::parse(content.begin(), content.end(), nullptr, false);
  const auto manifest = ParseGroupImpostorManifest(json);
  if (!manifest || manifest->Coverage != surface.Coverage ||
      manifest->SurfaceRevision != surface.Revision ||
      manifest->Origin.LongitudeDeg != surface.Origin.LongitudeDeg ||
      manifest->Origin.LatitudeDeg != surface.Origin.LatitudeDeg || !json.contains("card_assets") ||
      !json["card_assets"].is_array() ||
      json["card_assets"].size() != manifest->Directions.size()) {
    return result;
  }
  const auto eye = LocalEye(*manifest, demand.EyeEcef);
  if (!FitsProjection(*manifest, eye, demand.FocalPixels)) { return result; }
  const auto needed = SelectImpostorDirections(manifest->Directions, eye);
  if (needed.size() < 2) { return result; }
  const auto records = ReadViewRecords(cache, json["card_assets"], needed, meshBytes);
  if (!records) { return std::unexpected(records.error()); }
  if (records->empty()) { return result; }
  ResidentGroupImpostor group{
      .CentreM = manifest->CentreM, .HalfExtentM = manifest->HalfExtentM, .Views = {}};
  group.Views.reserve(needed.size());
  for (size_t at = 0; at < records->size(); ++at) {
    const auto &record = (*records)[at];
    result.RequestedBytes += static_cast<size_t>(record.ByteCount);
    const auto card = cache.Load(record.Key, GroupImpostorViewBytesMost);
    if (!card) { return std::unexpected("group impostor view read failed"); }
    if (!*card) { return result; }
    auto view = DecodeGroupImpostorView((**card).Bytes());
    if (!view || !MatchesView(*view, *manifest, needed[at])) { return result; }
    group.Views.push_back(std::move(*view));
  }
  result.AvailableViews = manifest->Directions.size();
  result.Product = std::move(group);
  return result;
}

}
