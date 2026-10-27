#ifndef OUTSHINE_GENERATORS_ROAD_PAGES_ROADPAGEPRODUCER_H
#define OUTSHINE_GENERATORS_ROAD_PAGES_ROADPAGEPRODUCER_H

#include "RoadPageProduct.h"
#include <cstddef>
#include <expected>
#include <stop_token>
#include <string>

namespace outshine::Generators {
class RoadPageProducer {
public:
  virtual ~RoadPageProducer() = default;
  [[nodiscard]] virtual size_t InputCapacityBytes() const noexcept = 0;
  [[nodiscard]] virtual std::expected<RoadPageGeometry, std::string>
  Produce(const RoadPageRequest &request, std::stop_token stop) const = 0;
};

}
#endif
