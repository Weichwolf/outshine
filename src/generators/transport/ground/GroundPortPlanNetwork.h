#ifndef OUTSHINE_GENERATORS_TRANSPORT_GROUND_GROUNDPORTPLANNETWORK_H
#define OUTSHINE_GENERATORS_TRANSPORT_GROUND_GROUNDPORTPLANNETWORK_H
#include "GroundPortField.h"
#include "TransportPlanNetwork.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <stop_token>
#include <variant>
#include <vector>

namespace outshine::Generators {
[[nodiscard]] std::expected<TransportPlanNetwork, TransportPlanNetworkError>
ProjectGroundPortFieldNetwork(const GroundPortField &field,
                              TransportPlanNetworkLimits limits = {},
                              const std::stop_token &stop = {});

class GroundPortPlanNetwork {
public:
  using Error = std::variant<TransportPlanNetworkError, TransportPlanCodecError>;

  [[nodiscard]] static std::expected<GroundPortPlanNetwork, Error>
  Prepare(const GroundPortField &field,
          TransportPlanNetworkLimits limits = {},
          size_t bindingBytesMost = 1048576,
          const std::stop_token &stop = {});

  [[nodiscard]] bool Matches(const GroundPortField &field) const noexcept;

  [[nodiscard]] const TransportPlanNetwork &Projection() const noexcept { return Projection_; }

  [[nodiscard]] const std::array<uint8_t, 32> &Binding() const noexcept { return Binding_; }

  [[nodiscard]] size_t CapacityBytes() const noexcept;

private:
  GroundPortPlanNetwork(const GroundPortField &field,
                        TransportPlanNetwork projection,
                        std::array<uint8_t, 32> binding);
  std::weak_ptr<const World::TransportNetworkSnapshot> Source_;
  std::vector<uint32_t> SourceEdges_;
  TransportPlanNetwork Projection_;
  std::array<uint8_t, 32> Binding_;
};
}
#endif
