#ifndef OUTSHINE_GENERATORS_ROAD_ROADPAGEMESHPRODUCER_H
#define OUTSHINE_GENERATORS_ROAD_ROADPAGEMESHPRODUCER_H

#include "RoadPageProducer.h"
#include "RoadPageRecipe.h"
#include <cstddef>
#include <expected>
#include <stop_token>
#include <string>

namespace outshine::Generators {
class RoadPageMeshProducer final : public RoadPageProducer {
public:
  RoadPageMeshProducer(std::string productKey, RoadPageRecipe recipe);

  [[nodiscard]] size_t InputCapacityBytes() const noexcept override { return InputBytes_; }

  [[nodiscard]] std::expected<RoadPageGeometry, std::string>
  Produce(const RoadPageRequest &request, std::stop_token stop) const override;

private:
  std::string ProductKey_;
  RoadPageRecipe Recipe_;
  size_t InputBytes_ = 0;
};
}
#endif
