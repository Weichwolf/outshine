#ifndef OUTSHINE_GENERATORS_ROAD_PAGES_STOREDROADPAGEMESHPRODUCER_H
#define OUTSHINE_GENERATORS_ROAD_PAGES_STOREDROADPAGEMESHPRODUCER_H

#include "PreparedRoadPages.h"
#include "RoadPageProducer.h"
#include <cstddef>
#include <expected>
#include <memory>
#include <stop_token>
#include <string>

namespace outshine::Generators {
class StoredRoadPageMeshProducer final : public RoadPageProducer {
public:
  [[nodiscard]] static std::expected<StoredRoadPageMeshProducer, std::string> Bind(
      std::shared_ptr<PreparedRoadPages> pages, RoadPageAddress address, AssetCache::ReadPlan read);

  [[nodiscard]] size_t InputCapacityBytes() const noexcept override { return InputBytes_; }

  [[nodiscard]] std::expected<RoadPageGeometry, std::string>
  Produce(const RoadPageRequest &request, std::stop_token stop) const override;

private:
  StoredRoadPageMeshProducer(std::shared_ptr<PreparedRoadPages> pages,
                             RoadPageAddress address,
                             AssetCache::ReadPlan read,
                             size_t decodedBytes);
  std::shared_ptr<PreparedRoadPages> Pages_;
  RoadPageAddress Address_;
  std::string Key_;
  AssetCache::ReadPlan Read_;
  size_t DecodedBytes_ = 0;
  size_t InputBytes_ = 0;
};
}
#endif
