#ifndef OUTSHINE_TEST_STOREGROUNDREGION_H
#define OUTSHINE_TEST_STOREGROUNDREGION_H
#include "PreparedGroundRegions.h"
#include <expected>
#include <string>
#include <utility>

namespace outshine::Test {
inline std::expected<Generators::Osm::PreparedGroundRegions::Loaded,
                     Generators::Osm::PreparedGroundRegions::Failure>
StoreGroundRegion(Generators::Osm::PreparedGroundRegions &cache,
                  const std::string &key,
                  const Box &bounds,
                  const GroundRegionAsset &region,
                  const Generators::WaterAsset &water,
                  const std::string &request = {}) {
  auto work = cache.PrepareStore(key, bounds, region, water, request);
  if (!work) { return std::unexpected(std::move(work.error())); }
  return cache.Store(*work);
}
}
#endif
