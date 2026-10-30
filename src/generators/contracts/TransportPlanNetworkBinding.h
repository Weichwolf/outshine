#ifndef OUTSHINE_GENERATORS_CONTRACTS_TRANSPORTPLANNETWORKBINDING_H
#define OUTSHINE_GENERATORS_CONTRACTS_TRANSPORTPLANNETWORKBINDING_H
#include "TransportPlanCodec.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>

namespace outshine::Generators {
[[nodiscard]] std::expected<std::array<uint8_t, 32>, TransportPlanCodecError>
TransportPlanNetworkBinding(const std::shared_ptr<const World::TransportNetworkSnapshot> &network,
                            size_t bytesMost);
}
#endif
