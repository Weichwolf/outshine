#ifndef OUTSHINE_GENERATORS_ROAD_PAGES_STOREDTRANSPORTROADPAGEPRODUCER_H
#define OUTSHINE_GENERATORS_ROAD_PAGES_STOREDTRANSPORTROADPAGEPRODUCER_H

#include "PreparedTransportPlans.h"
#include "RoadPageProducer.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <stop_token>
#include <string>
#include <vector>

namespace outshine::Generators {
struct StoredTransportRoadPageLimits {
  TransportPlanCodecLimits Plan;
  size_t ReadBufferBytesMost = 0;
};

class StoredTransportRoadPageProducer final : public RoadPageProducer {
public:
  [[nodiscard]] static std::expected<StoredTransportRoadPageProducer, std::string>
  Bind(std::shared_ptr<PreparedTransportPlans> plans,
       const TransportPlanDefinition &definition,
       const std::array<uint8_t, 32> &inputs,
       const RoadPageAddress &address,
       std::vector<RoadPagePartAppearance> parts,
       StoredTransportRoadPageLimits limits);

  // Reserves encoded/decoded input capacity before deferred plan loading; excludes mesher scratch.
  [[nodiscard]] size_t InputCapacityBytes() const noexcept override { return InputBytes_; }

  [[nodiscard]] std::expected<RoadPageGeometry, std::string>
  Produce(const RoadPageRequest &request, std::stop_token stop) const override;

private:
  StoredTransportRoadPageProducer(std::shared_ptr<PreparedTransportPlans> plans,
                                  TransportPlanDefinition definition,
                                  std::array<uint8_t, 32> inputs,
                                  std::string key,
                                  std::vector<RoadPagePartAppearance> parts,
                                  StoredTransportRoadPageLimits limits,
                                  size_t inputBytes);
  std::shared_ptr<PreparedTransportPlans> Plans_;
  TransportPlanDefinition Definition_;
  std::array<uint8_t, 32> Inputs_;
  std::string ProductKey_;
  std::vector<RoadPagePartAppearance> Parts_;
  StoredTransportRoadPageLimits Limits_;
  size_t InputBytes_ = 0;
};
}
#endif
