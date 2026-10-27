#ifndef OUTSHINE_GENERATORS_TRANSPORT_GROUND_GROUNDPORTCORE_H
#define OUTSHINE_GENERATORS_TRANSPORT_GROUND_GROUNDPORTCORE_H

#include "GroundPortField.h"
#include "TransportCoreSelection.h"
#include <expected>
#include <stop_token>

namespace outshine::Generators {
[[nodiscard]] std::expected<TransportCoreSelection, TransportCoreError>
SelectGroundPortCore(const GroundPortField &field,
                     GeoCellId core,
                     TransportCoreLimits limits,
                     const std::stop_token &stop = {});
}
#endif
