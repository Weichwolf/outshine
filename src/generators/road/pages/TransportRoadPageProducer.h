#ifndef OUTSHINE_GENERATORS_ROAD_PAGES_TRANSPORTROADPAGEPRODUCER_H
#define OUTSHINE_GENERATORS_ROAD_PAGES_TRANSPORTROADPAGEPRODUCER_H

#include "RoadPageProducer.h"
#include "TransportPlan.h"
#include <cstddef>
#include <expected>
#include <stop_token>
#include <string>
#include <vector>

namespace outshine::Generators {
class TransportRoadPageProducer final : public RoadPageProducer {
public:
  // The lease is validated and immutable; subsets require explicit matching demand.
  [[nodiscard]] static std::expected<TransportRoadPageProducer, std::string>
  Bind(TransportPlanLease plan,
       const RoadPageAddress &address,
       std::vector<RoadPagePartAppearance> parts,
       TransportRepresentation representation = TransportRepresentation::Functional);

  [[nodiscard]] size_t InputCapacityBytes() const noexcept override { return InputBytes_; }

  [[nodiscard]] std::expected<RoadPageGeometry, std::string>
  Produce(const RoadPageRequest &request, std::stop_token stop) const override;

private:
  TransportRoadPageProducer(TransportPlanLease plan,
                            std::string key,
                            std::vector<RoadPagePartAppearance> parts);
  TransportPlanLease Plan_;
  std::string ProductKey_;
  std::vector<RoadPagePartAppearance> Parts_;
  size_t InputBytes_ = 0;
};
}
#endif
