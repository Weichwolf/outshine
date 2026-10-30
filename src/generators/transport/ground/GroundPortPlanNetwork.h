#ifndef OUTSHINE_GENERATORS_TRANSPORT_GROUND_GROUNDPORTPLANNETWORK_H
#define OUTSHINE_GENERATORS_TRANSPORT_GROUND_GROUNDPORTPLANNETWORK_H
#include "GroundPortField.h"
#include "TransportPlanNetwork.h"
#include <expected>
#include <stop_token>

namespace outshine::Generators {
[[nodiscard]] std::expected<TransportPlanNetwork, TransportPlanNetworkError>
ProjectGroundPortFieldNetwork(const GroundPortField &field,
                              TransportPlanNetworkLimits limits = {},
                              const std::stop_token &stop = {});
}
#endif
