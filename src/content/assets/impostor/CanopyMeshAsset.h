#ifndef OUTSHINE_CONTENT_ASSETS_IMPOSTOR_CANOPYMESHASSET_H
#define OUTSHINE_CONTENT_ASSETS_IMPOSTOR_CANOPYMESHASSET_H

#include "SurfaceTile.h"
#include "ImpostorViewDemand.h"
#include <cstddef>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace outshine {

class AssetCache;
class PreparedAssetWrite;
struct AssetRecord;

struct CanopyMeshDetail {
  Vec3 CentreM;
  ImpostorDistanceRamp Ramp;
};

struct CanopyMesh {
  std::shared_ptr<const SurfaceTile> Prototype;
  std::vector<SurfaceInstance> Instances;
  std::shared_ptr<const SurfaceTile> CoarsePrototype;
  std::optional<CanopyMeshDetail> DetailHandoff;
  bool CoarseOnly = false;
};

struct CanopyMeshDemand {
  Vec3 CentreM;
  ImpostorEyeSweep Eye;
  double FocalPixels;
};

class CanopyMeshBank {
public:
  [[nodiscard]] std::expected<std::optional<CanopyMesh>, std::string>
  Read(const AssetCache &cache,
       const AssetRecord &group,
       const CanopyMeshDemand &demand,
       size_t &requestedBytes);

private:
  [[nodiscard]] std::expected<std::shared_ptr<const SurfaceTile>, std::string>
  ReadPrototype(const AssetCache &cache, const std::string &key, size_t &requestedBytes);

  std::unordered_map<std::string, std::shared_ptr<const SurfaceTile>> Meshes_;
};

[[nodiscard]] std::expected<void, std::string>
StageCanopyMesh(AssetCache &cache,
                const AssetRecord &group,
                const std::filesystem::path &directory,
                std::string_view layoutDigest,
                std::vector<PreparedAssetWrite> &writes);

}
#endif
