#include "CanopyMeshAsset.h"
#include "AssetCache.h"
#include "AssetPublication.h"
#include "ReadTextFile.h"
#include "Sha256.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>
#include <nlohmann/json.hpp>

namespace outshine {
namespace {
using Json = nlohmann::json;
constexpr std::string_view kBankKind = "render-canopy-instances-v1";
constexpr std::string_view kMeshKind = "render-canopy-mesh-v1";
constexpr size_t kLayoutBytes = 128 * size_t{1024};
constexpr size_t kMaximumInstances = 1024;
constexpr float kCoordinateLimitM = 2e7f;

std::span<const uint8_t> Bytes(std::string_view input) {
  return {reinterpret_cast<const uint8_t *>(input.data()), input.size()};
}

std::expected<void, std::string> Stage(AssetCache &cache,
                                       const AssetRecord &record,
                                       std::string_view data,
                                       std::vector<PreparedAssetWrite> &writes) {
  const auto bytes = Bytes(data);
  auto prepared = cache.PreparePublication({record}, {bytes.begin(), bytes.end()});
  if (!prepared) { return std::unexpected("canopy mesh package could not be staged"); }
  writes.push_back(std::move(*prepared));
  return {};
}

std::optional<std::vector<SurfaceInstance>> ReadInstances(const Json &layout) {
  const auto roots = layout.find("roots_m");
  const auto scales = layout.find("tree_scales");
  if (roots == layout.end() || scales == layout.end() || !roots->is_array() ||
      !scales->is_array() || roots->empty() || roots->size() != scales->size() ||
      roots->size() > kMaximumInstances) {
    return std::nullopt;
  }
  std::vector<SurfaceInstance> result;
  result.reserve(roots->size());
  for (size_t at = 0; at < roots->size(); ++at) {
    const auto &root = (*roots)[at];
    const auto &size = (*scales)[at];
    if (!root.is_array() || root.size() != 3 || !size.is_number() ||
        !std::ranges::all_of(root, &Json::is_number)) {
      return std::nullopt;
    }
    const SurfaceInstance row{
        root[0].get<float>(), root[1].get<float>(), root[2].get<float>(), size.get<float>()};
    if (row.back() <= 0 || !std::ranges::all_of(row, [](float value) {
          return std::abs(value) <= kCoordinateLimitM;
        })) {
      return std::nullopt;
    }
    result.push_back(row);
  }
  return result;
}

std::optional<SurfaceTile> DecodeMesh(std::string_view data) {
  auto tile = DecodePackedSurfaceTile(Bytes(data));
  if (!tile || tile->GridSide < 2 || !tile->GroundRoughness ||
      tile->GroundIndices >= tile->Indices.size()) {
    return std::nullopt;
  }
  tile->Indices.erase(tile->Indices.begin(), tile->Indices.begin() + tile->GroundIndices);
  tile->GroundIndices = 0;
  return std::move(*tile);
}

std::string Request(std::string_view key) {
  return Sha256Hex(std::format("{}/{}", kBankKind, key));
}

struct StagedPrototype {
  std::string Key;
  size_t Indices;
  double RadiusM;
};

double PrototypeRadius(const SurfaceTile &mesh) {
  const auto &low = mesh.MinimumM;
  const auto &high = mesh.MaximumM;
  return Length(Vec3{{std::max(std::abs(low[0]), std::abs(high[0])),
                      std::max(std::abs(low[1]), std::abs(high[1])),
                      std::max(std::abs(low[2]), std::abs(high[2]))}});
}

std::expected<StagedPrototype, std::string> StagePrototype(
    AssetCache &cache, const std::filesystem::path &path, std::vector<PreparedAssetWrite> &writes) {
  const auto mesh = ReadTextFile(path.string(), SurfaceTile::MaximumPackedBytes);
  const auto prototype = mesh ? DecodeMesh(*mesh) : std::nullopt;
  if (!prototype) { return std::unexpected("invalid canopy mesh prototype"); }
  const auto digest = Sha256Hex(*mesh);
  const AssetRecord shape{.Key = Sha256Hex(std::format("{}/{}", kMeshKind, digest)),
                          .Kind = std::string(kMeshKind),
                          .Bounds = {.Min = prototype->MinimumM, .Max = prototype->MaximumM},
                          .Package = digest,
                          .ByteCount = mesh->size(),
                          .Parent = {}};
  const auto prepared = Stage(cache, shape, *mesh, writes);
  if (!prepared) { return std::unexpected(prepared.error()); }
  return StagedPrototype{.Key = shape.Key,
                         .Indices = prototype->Indices.size(),
                         .RadiusM = PrototypeRadius(*prototype)};
}

std::optional<double> PositiveMetres(const Json &record, std::string_view field) {
  const auto value = record.find(field);
  if (value == record.end() || !value->is_number()) { return std::nullopt; }
  const double metres = value->get<double>();
  if (metres <= 0 || metres > kCoordinateLimitM) { return std::nullopt; }
  return metres;
}

std::expected<void, std::string> StageCoarse(AssetCache &cache,
                                             const std::filesystem::path &directory,
                                             const StagedPrototype &fine,
                                             Json &bank,
                                             std::vector<PreparedAssetWrite> &writes) {
  const auto path = directory / "near-coarse.surface";
  const auto metadataPath = directory / "near-coarse.json";
  std::error_code error;
  const bool hasMesh = std::filesystem::exists(path, error);
  if (error) { return std::unexpected("could not inspect coarse canopy mesh"); }
  const bool hasMetadata = std::filesystem::exists(metadataPath, error);
  if (error) { return std::unexpected("could not inspect coarse canopy metadata"); }
  if (!hasMesh && !hasMetadata) { return {}; }
  const auto metadata = ReadTextFile(metadataPath.string(), kLayoutBytes);
  const auto cell =
      metadata ? PositiveMetres(Json::parse(*metadata, nullptr, false), "cell_m") : std::nullopt;
  if (!cell) { return std::unexpected("coarse canopy requires its authored cell size"); }
  const auto coarse = StagePrototype(cache, path, writes);
  if (!coarse) { return std::unexpected(coarse.error()); }
  if (coarse->Indices >= fine.Indices) {
    return std::unexpected("coarse canopy must reduce prototype work");
  }
  bank["coarse_mesh"] = coarse->Key;
  bank["coarse_cell_m"] = *cell;
  bank["prototype_radius_m"] = std::max(fine.RadiusM, coarse->RadiusM);
  return {};
}

std::optional<ImpostorDistanceRamp> MeshDetailRamp(const Json &bank,
                                                   std::span<const SurfaceInstance> instances,
                                                   const CanopyMeshDemand &demand) {
  constexpr double kFineCellPx = 1.2;
  constexpr double kCoarseCellPx = 1.1;
  const auto cell = PositiveMetres(bank, "coarse_cell_m");
  const auto radius = PositiveMetres(bank, "prototype_radius_m");
  if (!cell || !radius || !(demand.FocalPixels > 0)) { return std::nullopt; }
  double supportM = 0;
  double scale = 0;
  for (const auto &instance : instances) {
    const Vec3 root{{instance[0], instance[1], instance[2]}};
    scale = std::max(scale, double{instance[3]});
    supportM = std::max(supportM, Length(root - demand.CentreM) + *radius * instance[3]);
  }
  const double pixelsAtOneMetre = *cell * scale * demand.FocalPixels;
  return ImpostorDistanceRamp{.BeginM = supportM + pixelsAtOneMetre / kFineCellPx,
                              .EndM = supportM + pixelsAtOneMetre / kCoarseCellPx};
}

const std::string *ReadKey(const Json &bank, std::string_view field) {
  const auto value = bank.find(field);
  if (value == bank.end() || !value->is_string()) { return nullptr; }
  const auto &key = value->get_ref<const std::string &>();
  return AssetProductKey::Parse(key) ? &key : nullptr;
}
}

std::expected<void, std::string> StageCanopyMesh(AssetCache &cache,
                                                 const AssetRecord &group,
                                                 const std::filesystem::path &directory,
                                                 std::string_view layoutDigest,
                                                 std::vector<PreparedAssetWrite> &writes) {
  const auto layoutPath = directory / "near-layout.json";
  const auto meshPath = directory / "near.surface";
  std::error_code error;
  const bool hasLayout = std::filesystem::exists(layoutPath, error);
  if (error) { return std::unexpected("could not inspect canopy layout"); }
  const bool hasMesh = std::filesystem::exists(meshPath, error);
  if (error) { return std::unexpected("could not inspect canopy mesh"); }
  if (!hasLayout && !hasMesh) { return {}; }
  const auto layout = ReadTextFile(layoutPath.string(), kLayoutBytes);
  if (!layout || !AssetProductKey::Parse(layoutDigest) || Sha256Hex(*layout) != layoutDigest) {
    return std::unexpected("near canopy must share the exact baked layout");
  }
  auto instances = Json::parse(*layout, nullptr, false);
  if (!ReadInstances(instances)) { return std::unexpected("invalid near canopy instances"); }
  const auto fine = StagePrototype(cache, meshPath, writes);
  if (!fine) { return std::unexpected(fine.error()); }
  Json bankBody{{"mesh", fine->Key},
                {"roots_m", std::move(instances["roots_m"])},
                {"tree_scales", std::move(instances["tree_scales"])}};
  const auto coarse = StageCoarse(cache, directory, *fine, bankBody, writes);
  if (!coarse) { return coarse; }
  const auto body = bankBody.dump();
  if (body.size() > kLayoutBytes) { return std::unexpected("near canopy instance bank too large"); }
  const auto request = Request(group.Key);
  const auto content = Sha256Hex(body);
  const AssetRecord bank{.Key = Sha256Hex(request + content),
                         .Kind = std::string(kBankKind),
                         .Bounds = group.Bounds,
                         .Package = content,
                         .ByteCount = body.size(),
                         .Level = group.Level,
                         .Parent = group.Key,
                         .RequestKey = request};
  return Stage(cache, bank, body, writes);
}

std::expected<std::optional<CanopyMesh>, std::string>
CanopyMeshBank::Read(const AssetCache &cache,
                     const AssetRecord &group,
                     const CanopyMeshDemand &demand,
                     size_t &requestedBytes) {
  const auto descriptor = cache.LoadRequest(Request(group.Key), kLayoutBytes);
  if (!descriptor) { return std::unexpected("near canopy lookup failed"); }
  if (!*descriptor || (**descriptor).Record().Kind != kBankKind ||
      (**descriptor).Record().Parent != group.Key) {
    return std::nullopt;
  }
  const auto bytes = (**descriptor).Bytes();
  requestedBytes += bytes.size();
  const auto layout = Json::parse(bytes.begin(), bytes.end(), nullptr, false);
  auto instances = ReadInstances(layout);
  const auto *fine = ReadKey(layout, "mesh");
  if (!instances || fine == nullptr) { return std::nullopt; }
  const auto *coarse = ReadKey(layout, "coarse_mesh");
  const auto ramp = coarse == nullptr ? std::nullopt : MeshDetailRamp(layout, *instances, demand);
  const auto mode =
      ramp ? ClassifyImpostorSweep(demand.Eye, *ramp) : ImpostorDistanceMode::Geometry;
  const bool coarseFirst = mode == ImpostorDistanceMode::Impostor;
  auto selected = ReadPrototype(cache, coarseFirst ? *coarse : *fine, requestedBytes);
  if (!selected) { return std::unexpected(selected.error()); }
  CanopyMesh mesh{.Prototype = std::move(*selected),
                  .Instances = std::move(*instances),
                  .CoarsePrototype = {},
                  .DetailHandoff = {},
                  .CoarseOnly = coarseFirst};
  if (coarse != nullptr && (!mesh.Prototype || mode == ImpostorDistanceMode::Handoff)) {
    auto other = ReadPrototype(cache, coarseFirst ? *fine : *coarse, requestedBytes);
    if (!other) { return std::unexpected(other.error()); }
    if (!mesh.Prototype) {
      mesh.Prototype = std::move(*other);
      mesh.CoarseOnly = !coarseFirst;
    } else if (*other && ramp) {
      mesh.CoarsePrototype = std::move(*other);
      mesh.DetailHandoff = CanopyMeshDetail{.CentreM = demand.CentreM, .Ramp = *ramp};
    }
  }
  if (!mesh.Prototype) { return std::nullopt; }
  return mesh;
}

std::expected<std::shared_ptr<const SurfaceTile>, std::string> CanopyMeshBank::ReadPrototype(
    const AssetCache &cache, const std::string &key, size_t &requestedBytes) {
  auto [held, read] = Meshes_.try_emplace(key);
  if (read) {
    const auto canonical = cache.Load(key, SurfaceTile::MaximumPackedBytes);
    if (!canonical) { return std::unexpected("near canopy prototype read failed"); }
    if (*canonical && (**canonical).Record().Kind == kMeshKind) {
      const auto raw = (**canonical).Bytes();
      requestedBytes += raw.size();
      auto prototype = DecodeMesh({reinterpret_cast<const char *>(raw.data()), raw.size()});
      if (prototype) { held->second = std::make_shared<const SurfaceTile>(std::move(*prototype)); }
    }
  }
  return held->second;
}

}
