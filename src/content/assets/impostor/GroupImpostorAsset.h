#ifndef OUTSHINE_CONTENT_ASSETS_IMPOSTOR_GROUPIMPOSTORASSET_H
#define OUTSHINE_CONTENT_ASSETS_IMPOSTOR_GROUPIMPOSTORASSET_H

#include "GroupImpostorView.h"
#include "ImpostorViewDemand.h"
#include "SurfaceTile.h"
#include "Vec3.h"
#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace outshine {

class AssetCache;
struct AssetRecord;

struct GroupImpostorDemand {
  ImpostorEyeSweep EyeEcef;
  double FocalPixels = 0;
  bool ReplaceSurface = true;
};

struct ResidentGroupImpostor {
  Vec3 CentreM;
  double HalfExtentM;
  std::vector<GroupImpostorView> Views;
  std::optional<ImpostorDistanceRamp> Handoff;
  bool FractionalCoverage = false;
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
  ResidentGroupImpostor Group;
  size_t AvailableViews;
};

struct AttachedCanopyLookup {
  std::vector<AttachedCanopyGroup> Groups;
  size_t RequestedBytes = 0;
};

[[nodiscard]] std::expected<void, std::string>
ImportGroupImpostor(AssetCache &cache, const GroupImpostorImport &source);

[[nodiscard]] std::expected<GroupImpostorLookup, std::string> LoadGroupImpostor(
    const AssetCache &cache, const AssetRecord &surface, const GroupImpostorDemand &demand);

[[nodiscard]] std::expected<AttachedCanopyLookup, std::string> LoadAttachedCanopy(
    const AssetCache &cache, const AssetRecord &surface, const ImpostorEyeSweep &eye);

}
#endif
