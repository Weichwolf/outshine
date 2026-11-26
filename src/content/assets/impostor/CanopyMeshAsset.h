#ifndef OUTSHINE_CONTENT_ASSETS_IMPOSTOR_CANOPYMESHASSET_H
#define OUTSHINE_CONTENT_ASSETS_IMPOSTOR_CANOPYMESHASSET_H

#include "SurfaceTile.h"
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

struct CanopyMesh {
  std::shared_ptr<const SurfaceTile> Prototype;
  std::vector<SurfaceInstance> Instances;
};

class CanopyMeshBank {
public:
  [[nodiscard]] std::expected<std::optional<CanopyMesh>, std::string>
  Read(const AssetCache &cache, const AssetRecord &group, size_t &requestedBytes);

private:
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
