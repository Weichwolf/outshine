#ifndef OUTSHINE_TEST_CONTROLLEDROADPAGEPRODUCER_H
#define OUTSHINE_TEST_CONTROLLEDROADPAGEPRODUCER_H

#include "RoadPageProducer.h"
#include <cstddef>
#include <functional>
#include <memory>
#include <utility>

namespace outshine::Test {
template <typename Function>
class ControlledRoadPageProducer final : public Generators::RoadPageProducer {
public:
  explicit ControlledRoadPageProducer(Function function) : Function_(std::move(function)) {}

  [[nodiscard]] size_t InputCapacityBytes() const noexcept override { return sizeof(*this); }

  [[nodiscard]] std::expected<Generators::RoadPageGeometry, std::string>
  Produce(const Generators::RoadPageRequest &request, std::stop_token stop) const override {
    return std::invoke(Function_, request, stop);
  }

private:
  Function Function_;
};

template <typename Function>
[[nodiscard]] std::shared_ptr<const Generators::RoadPageProducer> RoadProducer(Function function) {
  return std::make_shared<const ControlledRoadPageProducer<Function>>(std::move(function));
}
}
#endif
