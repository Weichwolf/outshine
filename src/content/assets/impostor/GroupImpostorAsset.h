#ifndef OUTSHINE_CONTENT_ASSETS_IMPOSTOR_GROUPIMPOSTORASSET_H
#define OUTSHINE_CONTENT_ASSETS_IMPOSTOR_GROUPIMPOSTORASSET_H

#include "Box.h"
#include "CanopyMeshAsset.h"
#include "GroupImpostorView.h"
#include "ImpostorViewDemand.h"
#include "SurfaceTile.h"
#include "Vec3.h"
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace outshine {

class AssetCache;
struct AssetRecord;

struct ResidentGroupImpostor {
  std::string ManifestKey;
  Vec3 CentreM;
  double HalfExtentM;
  std::shared_ptr<const std::vector<GroupImpostorView>> Views;
  std::optional<ImpostorDistanceRamp> Handoff;
  bool FractionalCoverage = false;
};

using ResidentImpostorGroups = std::unordered_map<std::string_view, const ResidentGroupImpostor *>;

struct GroupImpostorDemand {
  ImpostorEyeSweep EyeEcef;
  double FocalPixels = 0;
  bool PreferCoarse = false;
  std::span<const std::string> PanoramaGroups;
  const ResidentImpostorGroups *Resident = nullptr;
};

struct GroupImpostorSurface {
  std::optional<SurfaceTile> Ground;
  ResidentGroupImpostor Group;
  size_t AvailableViews;
};

struct GroupImpostorLookup {
  std::optional<GroupImpostorSurface> Product;
  size_t RequestedBytes = 0;
};

struct GroupImpostorImport {
  std::string_view Revision;
  std::string_view Directory;
};

struct AttachedCanopyGroup {
  std::string Key;
  LongitudeLatitude Origin;
  std::optional<ResidentGroupImpostor> Group;
  std::optional<CanopyMesh> Near;
  size_t AvailableViews;
};

struct AttachedCanopyLookup {
  std::vector<AttachedCanopyGroup> Groups;
  size_t RequestedBytes = 0;
  size_t NearRequestedBytes = 0;
  bool NeedsRefinement = false;
};

[[nodiscard]] std::expected<void, std::string>
ImportGroupImpostor(AssetCache &cache, const GroupImpostorImport &source);

[[nodiscard]] std::expected<GroupImpostorLookup, std::string> LoadGroupImpostor(
    const AssetCache &cache, const AssetRecord &surface, const GroupImpostorDemand &demand);

struct CanopyCutDemand {
  std::string_view Revision;
  Box Bounds;
  std::span<const SurfaceCoverageInterval> Ground;
  const GroupImpostorDemand &Impostors;
};

[[nodiscard]] std::expected<AttachedCanopyLookup, std::string>
LoadCanopyCut(const AssetCache &cache, const CanopyCutDemand &demand);

}
#endif
