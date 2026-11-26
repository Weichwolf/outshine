#ifndef OUTSHINE_CONTENT_ASSETS_FARPANORAMAASSET_H
#define OUTSHINE_CONTENT_ASSETS_FARPANORAMAASSET_H

#include "FarPanorama.h"
#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace outshine {

class AssetCache;

struct FarPanoramaLookup {
  std::vector<FarPanorama> Products;
  size_t RequestedBytes = 0;
};

[[nodiscard]] std::expected<void, std::string>
PublishFarPanorama(AssetCache &cache, std::string_view revision, const FarPanorama &panorama);

[[nodiscard]] std::expected<FarPanoramaLookup, std::string>
LoadFarPanoramas(AssetCache &cache,
                 std::string_view revision,
                 LongitudeLatitude origin,
                 const Vec3 &startM,
                 const Vec3 &finishM);

}

#endif
