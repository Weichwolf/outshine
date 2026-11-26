#include "GroupImpostorAsset.h"
#include "AssetCache.h"
#include "AssetPublication.h"
#include "GroupImpostorManifest.h"
#include "GroupImpostorView.h"
#include "ImpostorViewDemand.h"
#include "ReadTextFile.h"
#include "Sha256.h"
#include "SurfaceTile.h"
#include "SurfaceTileAsset.h"
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
                                                  std::string_view owner,
                                                  size_t at,
                                                  std::vector<PreparedAssetWrite> &writes) {
  const auto file = ReadTextFile(path.string(), GroupImpostorViewBytesMost);
  if (!file) { return std::unexpected(file.error()); }
  const auto view = DecodeGroupImpostorView(Bytes(*file));
  if (!view || !MatchesView(*view, manifest, at)) {
    return std::unexpected("group view differs from its capture manifest");
  }
  const auto payload = PackageKey(Bytes(*file));
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

std::expected<std::optional<CachedAsset>, std::string> ReadManifest(const AssetCache &cache,
                                                                    std::string_view sourceKey) {
  auto manifest =
      cache.LoadRequest(GroupImpostorRequestKey(sourceKey), GroupImpostorManifestBytesMost);
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

bool MatchesSurface(const SurfaceTile &surface, const GroupImpostorManifest &group) {
  return surface.Coverage == group.Coverage && surface.Revision == group.SurfaceRevision &&
         surface.Origin.LongitudeDeg == group.Origin.LongitudeDeg &&
         surface.Origin.LatitudeDeg == group.Origin.LatitudeDeg;
}

std::expected<void, std::string> StageGround(AssetCache &cache,
                                             const CachedAsset &source,
                                             const GroupImpostorManifest &manifest,
                                             Json &index,
                                             std::vector<PreparedAssetWrite> &writes) {
  const auto surface = DecodePackedSurfaceTile(source.Bytes());
  if (!surface || !MatchesSurface(*surface, manifest)) {
    return std::unexpected("group capture does not match its source surface");
  }
  const size_t overlayBytes = OverlayBytes(*surface);
  const auto ground = EncodeSurfaceGround(*surface);
  if (overlayBytes == 0 || !ground) {
    return std::unexpected("group source has no separable ground");
  }
  const auto hash = PackageKey(*ground);
  const auto &original = source.Record();
  const AssetRecord record{
      .Key = Sha256Hex(std::format("{}/{}/{}", GroupImpostorGroundKind, original.Key, hash)),
      .Kind = std::string(GroupImpostorGroundKind),
      .Bounds = original.Bounds,
      .Package = hash,
      .ByteCount = ground->size(),
      .Level = original.Level,
      .Parent = {}};
  auto write = PreparePackage(cache, record, *ground);
  if (!write) { return std::unexpected(write.error()); }
  writes.push_back(std::move(*write));
  index["ground_asset"] = record.Key;
  index["source_asset"] = original.Key;
  index["overlay_bytes"] = overlayBytes;
  return {};
}

std::expected<std::optional<SurfaceTile>, std::string>
ReadGround(const AssetCache &cache,
           const Json &key,
           const GroupImpostorManifest &manifest,
           size_t &requestedBytes) {
  if (!key.is_string()) { return std::nullopt; }
  const auto &id = key.get_ref<const std::string &>();
  if (!AssetProductKey::Parse(id)) { return std::nullopt; }
  const auto package = cache.Load(id, SurfaceTile::MaximumPackedBytes);
  if (!package) { return std::unexpected("group ground read failed"); }
  if (!*package || (**package).Record().Kind != GroupImpostorGroundKind) { return std::nullopt; }
  requestedBytes += (**package).Bytes().size();
  auto ground = DecodePackedSurfaceTile((**package).Bytes());
  if (!ground || !MatchesSurface(*ground, manifest) ||
      ground->GroundIndices != ground->Indices.size()) {
    return std::nullopt;
  }
  return std::move(*ground);
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
  const auto original = cache.LoadRequest(
      SurfaceTileRequestKey(source.Revision, manifest->Coverage), SurfaceTile::MaximumPackedBytes);
  if (!original || !*original) {
    return std::unexpected("group requires its indexed source surface");
  }
  std::vector<PreparedAssetWrite> writes;
  writes.reserve(manifest->Directions.size() + 2);
  const auto ground = StageGround(cache, **original, *manifest, manifestJson, writes);
  if (!ground) { return std::unexpected(ground.error()); }
  const auto request = GroupImpostorRequestKey((**original).Record().Key);
  auto cards = Json::array();
  for (size_t at = 0; at < manifest->Directions.size(); ++at) {
    const auto card =
        StageView(cache, folder / std::format("{}.card", at), *manifest, request, at, writes);
    if (!card) { return std::unexpected(card.error()); }
    cards.push_back(card->Key);
  }
  manifestJson["card_assets"] = std::move(cards);
  const auto body = manifestJson.dump();
  if (body.size() > GroupImpostorManifestBytesMost) {
    return std::unexpected("oversized group impostor manifest");
  }
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

std::expected<GroupImpostorLookup, std::string> LoadGroupImpostor(
    const AssetCache &cache, const AssetRecord &surface, const GroupImpostorDemand &demand) {
  GroupImpostorLookup result;
  const auto loaded = ReadManifest(cache, surface.Key);
  if (!loaded) { return std::unexpected(loaded.error()); }
  if (!*loaded) { return result; }
  const auto content = (**loaded).Bytes();
  result.RequestedBytes = content.size();
  const auto json = Json::parse(content.begin(), content.end(), nullptr, false);
  const auto manifest = ParseGroupImpostorManifest(json);
  if (!manifest || !json.contains("source_asset") || json["source_asset"] != surface.Key ||
      !json.contains("ground_asset") || !json.contains("overlay_bytes") ||
      !json["overlay_bytes"].is_number_unsigned() || !json.contains("card_assets") ||
      !json["card_assets"].is_array() ||
      json["card_assets"].size() != manifest->Directions.size()) {
    return result;
  }
  const auto meshBytes = json["overlay_bytes"].get<size_t>();
  if (meshBytes == 0 || meshBytes >= surface.ByteCount) { return result; }
  if (demand.FocalPixels <= 0) { return result; }
  const auto eye = LocalEye(*manifest, demand.EyeEcef);
  const auto ramp =
      ImpostorProjectionRamp(manifest->HalfExtentM, manifest->Side, demand.FocalPixels);
  const auto mode = ClassifyImpostorSweep(eye, ramp);
  if (mode == ImpostorDistanceMode::Geometry) { return result; }
  const auto needed = SelectImpostorDirections(manifest->Directions, eye);
  if (needed.size() < 2) { return result; }
  const auto records = ReadViewRecords(cache, json["card_assets"], needed, meshBytes);
  if (!records) { return std::unexpected(records.error()); }
  if (records->empty()) { return result; }
  ResidentGroupImpostor group{.CentreM = manifest->CentreM,
                              .HalfExtentM = manifest->HalfExtentM,
                              .Views = {},
                              .Handoff = {}};
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
  std::optional<SurfaceTile> ground;
  if (mode == ImpostorDistanceMode::Handoff) {
    group.Handoff = ramp;
  } else {
    auto terrain = ReadGround(cache, json["ground_asset"], *manifest, result.RequestedBytes);
    if (!terrain) { return std::unexpected(terrain.error()); }
    if (!*terrain) { return result; }
    ground = std::move(*terrain);
  }
  result.Product = GroupImpostorSurface{.Ground = std::move(ground),
                                        .Group = std::move(group),
                                        .AvailableViews = manifest->Directions.size()};
  return result;
}

}
