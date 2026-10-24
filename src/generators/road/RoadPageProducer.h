#ifndef OUTSHINE_GENERATORS_ROAD_ROADPAGEPRODUCER_H
#define OUTSHINE_GENERATORS_ROAD_ROADPAGEPRODUCER_H

#include "RoadPageProduct.h"
#include <expected>
#include <stop_token>
#include <string>

namespace outshine::Generators {
class RoadPageProducer {
public:
  virtual ~RoadPageProducer() = default;
  [[nodiscard]] virtual std::expected<RoadPageGeometry, std::string>
  Produce(const RoadPageRequest &request, std::stop_token stop) const = 0;
};

class RoadPageMeshProducer final : public RoadPageProducer {
public:
  RoadPageMeshProducer(std::string productKey, RoadPageRecipe recipe);
  [[nodiscard]] std::expected<RoadPageGeometry, std::string>
  Produce(const RoadPageRequest &request, std::stop_token stop) const override;

private:
  std::string ProductKey_;
  RoadPageRecipe Recipe_;
};
}
#endif
