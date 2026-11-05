#ifndef OUTSHINE_GENERATORS_ROAD_PAGES_ROADPAGEPRODUCT_H
#define OUTSHINE_GENERATORS_ROAD_PAGES_ROADPAGEPRODUCT_H

#include "AssetCache.h"
#include "RoadPageAsset.h"
#include "RoadDisplaySampler.h"
#include <cstddef>
#include <memory>
#include <optional>
#include <string>

namespace outshine::Generators {
struct RoadPageRequest {
  RoadPageAddress Address;
  std::string Parent;
  std::string RequestKey;
  size_t NativeBytesMost = 0;
  std::optional<RoadDisplayLimits> SweepLimits = std::nullopt;
};

struct RoadPageLease {
  std::unique_ptr<RoadPageAsset> Page;
  AssetRecord Record;
  size_t NativeBytes = 0;
};
}
#endif
