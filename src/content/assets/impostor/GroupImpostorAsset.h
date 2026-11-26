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
};

struct ResidentGroupImpostor {
  Vec3 CentreM;
  double HalfExtentM;
  std::vector<GroupImpostorView> Views;
  std::optional<ImpostorDistanceRamp> Handoff;
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

[[nodiscard]] std::expected<void, std::string>
ImportGroupImpostor(AssetCache &cache, const GroupImpostorImport &source);

[[nodiscard]] std::expected<GroupImpostorLookup, std::string> LoadGroupImpostor(
    const AssetCache &cache, const AssetRecord &surface, const GroupImpostorDemand &demand);

}
#endif
