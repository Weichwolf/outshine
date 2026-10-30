#include "TransportPlanNetworkBinding.h"
#include "TransportPlanFormat.h"
#include "TransportNetworkSnapshot.h"
#include "Sha256.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>

namespace outshine::Generators {
std::expected<std::array<uint8_t, 32>, TransportPlanCodecError>
TransportPlanNetworkBinding(const std::shared_ptr<const World::TransportNetworkSnapshot> &network,
                            size_t bytesMost) {
  if (!network || !World::ValidateTransportNetwork(*network)) {
    return std::unexpected(TransportPlanCodecError::InvalidNetwork);
  }
  BinaryValueWriter out(bytesMost);
  if (!out.Text("outshine/transport-plan-network/1") || !TransportFormat::Network(out, network)) {
    return std::unexpected(TransportPlanCodecError::CapacityExceeded);
  }
  return Sha256Digest(out.Out.Bytes().data(), out.Out.Bytes().size());
}
}
