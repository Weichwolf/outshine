#ifndef OUTSHINE_CONTENT_ASSETS_IMPOSTOR_GROUPIMPOSTORMANIFEST_H
#define OUTSHINE_CONTENT_ASSETS_IMPOSTOR_GROUPIMPOSTORMANIFEST_H

#include "Box.h"
#include "Earth.h"
#include "TileId.h"
#include "Vec3.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <nlohmann/json_fwd.hpp>

namespace outshine {

inline constexpr std::string_view GroupImpostorManifestKind = "render-group-impostor-v1";
inline constexpr std::string_view GroupImpostorViewKind = "render-group-impostor-view-v1";
inline constexpr size_t GroupImpostorManifestBytesMost = size_t{16} * 1024;
inline constexpr size_t GroupImpostorViewBytesMost = size_t{4} * 1024 * 1024;

struct GroupImpostorManifest {
  Data::TileId Coverage;
  LongitudeLatitude Origin;
  Vec3 CentreM;
  double HalfExtentM;
  uint32_t Side;
  std::string SurfaceRevision;
  std::vector<Vec3> Directions;
};

[[nodiscard]] std::optional<GroupImpostorManifest>
ParseGroupImpostorManifest(const nlohmann::json &root);

[[nodiscard]] Box GroupImpostorEarthBounds(const GroupImpostorManifest &manifest);
[[nodiscard]] std::string GroupImpostorRequestKey(std::string_view revision, Data::TileId tile);

}
#endif
